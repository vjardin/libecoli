/*
 * Copyright (c) 2016-2017, Olivier MATZ <zer0@droids-corp.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University of California, Berkeley nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_tk_expr.h>
#include <ecoli_tk_str.h>
#include <ecoli_tk_or.h>
#include <ecoli_tk_int.h>
#include <ecoli_tk_many.h>
#include <ecoli_tk_seq.h>
#include <ecoli_tk_option.h>
#include <ecoli_tk_re.h>
#include <ecoli_tk_re_lex.h>
#include <ecoli_tk_cmd.h>

struct ec_tk_cmd {
	struct ec_tk gen;
	char *cmd_str;           /* the command string. */
	struct ec_tk *cmd;       /* the command token. */
	struct ec_tk *lex;       /* the lexer token. */
	struct ec_tk *expr;      /* the expression parser. */
	struct ec_tk **table;    /* table of tk referenced in command. */
	unsigned int len;        /* len of the table. */
};

static int
ec_tk_cmd_eval_var(void **result, void *userctx,
	const struct ec_parsed_tk *var)
{
	const struct ec_strvec *vec;
	struct ec_tk_cmd *tk = userctx;
	struct ec_tk *eval = NULL;
	const char *str, *id;
	unsigned int i;

	(void)userctx;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parsed_tk_strvec(var);
	if (ec_strvec_len(vec) != 1)
		return -EINVAL;
	str = ec_strvec_val(vec, 0);

	for (i = 0; i < tk->len; i++) {
		id = ec_tk_id(tk->table[i]);
		printf("i=%d id=%s\n", i, id);
		if (id == NULL)
			continue;
		if (strcmp(str, id))
			continue;
		/* if id matches, use a tk provided by the user... */
		eval = ec_tk_clone(tk->table[i]);
		if (eval == NULL)
			return -ENOMEM;
		break;
	}

	/* ...or create a string token */
	if (eval == NULL) {
		eval = ec_tk_str(NULL, str);
		if (eval == NULL)
			return -ENOMEM;
	}

	printf("eval var %s %p\n", str, eval);
	*result = eval;

	return 0;
}

static int
ec_tk_cmd_eval_pre_op(void **result, void *userctx, void *operand,
	const struct ec_parsed_tk *operator)
{
	(void)result;
	(void)userctx;
	(void)operand;
	(void)operator;

	return -EINVAL;
}

static int
ec_tk_cmd_eval_post_op(void **result, void *userctx, void *operand,
	const struct ec_parsed_tk *operator)
{
	const struct ec_strvec *vec;
	struct ec_tk *eval = operand;;

	(void)userctx;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parsed_tk_strvec(operator);
	if (ec_strvec_len(vec) != 1)
		return -EINVAL;

	if (!strcmp(ec_strvec_val(vec, 0), "*"))
		eval = NULL; //XXX
	else
		return -EINVAL;

	printf("eval post_op %p\n", eval);
	*result = eval;

	return 0;
}

static int
ec_tk_cmd_eval_bin_op(void **result, void *userctx, void *operand1,
	const struct ec_parsed_tk *operator, void *operand2)

{
	const struct ec_strvec *vec;
	struct ec_tk *out = NULL;
	struct ec_tk *in1 = operand1;
	struct ec_tk *in2 = operand2;

	(void)userctx;

	printf("eval bin_op %p %p\n", in1, in2);

	/* get parsed string vector, it should contain only one str */
	vec = ec_parsed_tk_strvec(operator);
	if (ec_strvec_len(vec) != 1)
		return -EINVAL;

	if (!strcmp(ec_strvec_val(vec, 0), "|")) {
		out = EC_TK_OR(NULL, ec_tk_clone(in1), ec_tk_clone(in2));
		if (out == NULL)
			return -EINVAL;
		ec_tk_free(in1);
		ec_tk_free(in2);
		*result = out;
	} else if (!strcmp(ec_strvec_val(vec, 0), ",")) {
		ec_tk_free(in1);
		ec_tk_free(in2);
		*result = NULL; //XXX
	} else {
		return -EINVAL;
	}

	return 0;
}

static int
ec_tk_cmd_eval_parenthesis(void **result, void *userctx,
	const struct ec_parsed_tk *open_paren,
	const struct ec_parsed_tk *close_paren,
	void *value)
{
	const struct ec_strvec *vec;
	struct ec_tk *in = value;;
	struct ec_tk *out = NULL;;

	(void)userctx;
	(void)close_paren;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parsed_tk_strvec(open_paren);
	if (ec_strvec_len(vec) != 1)
		return -EINVAL;

	if (!strcmp(ec_strvec_val(vec, 0), "[")) {
		out = ec_tk_option_new(NULL, ec_tk_clone(in));
		if (out == NULL)
			return -EINVAL;
		ec_tk_free(in);
	} else {
		return -EINVAL;
	}

	printf("eval paren\n");
	*result = out;

	return 0;
}

static void
ec_tk_cmd_eval_free(void *result, void *userctx)
{
	(void)userctx;
	ec_free(result);
}

static const struct ec_tk_expr_eval_ops test_ops = {
	.eval_var = ec_tk_cmd_eval_var,
	.eval_pre_op = ec_tk_cmd_eval_pre_op,
	.eval_post_op = ec_tk_cmd_eval_post_op,
	.eval_bin_op = ec_tk_cmd_eval_bin_op,
	.eval_parenthesis = ec_tk_cmd_eval_parenthesis,
	.eval_free = ec_tk_cmd_eval_free,
};

static struct ec_parsed_tk *ec_tk_cmd_parse(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_cmd *tk = (struct ec_tk_cmd *)gen_tk;

	return ec_tk_parse_tokens(tk->cmd, strvec);
}

static struct ec_completed_tk *ec_tk_cmd_complete(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_cmd *tk = (struct ec_tk_cmd *)gen_tk;

	return ec_tk_complete_tokens(tk->cmd, strvec);
}

static void ec_tk_cmd_free_priv(struct ec_tk *gen_tk)
{
	struct ec_tk_cmd *tk = (struct ec_tk_cmd *)gen_tk;
	unsigned int i;

	ec_free(tk->cmd_str);
	ec_tk_free(tk->cmd);
	ec_tk_free(tk->expr);
	ec_tk_free(tk->lex);
	for (i = 0; i < tk->len; i++)
		ec_tk_free(tk->table[i]);
	ec_free(tk->table);
}

static int ec_tk_cmd_build(struct ec_tk *gen_tk)
{
	struct ec_tk *expr = NULL, *lex = NULL, *cmd = NULL;
	struct ec_parsed_tk *p, *child;
	struct ec_tk_cmd *tk = (struct ec_tk_cmd *)gen_tk;
	void *result;
	int ret;

	/* build the expression parser */
	ret = -ENOMEM;
	expr = ec_tk_expr("expr");
	if (expr == NULL)
		goto fail;
	ret = ec_tk_expr_set_val_tk(expr, ec_tk_re(NULL, "[a-zA-Z0-9]+"));
	if (ret < 0)
		goto fail;
	ret = ec_tk_expr_add_bin_op(expr, ec_tk_str(NULL, ","));
	if (ret < 0)
		goto fail;
	ret = ec_tk_expr_add_bin_op(expr, ec_tk_str(NULL, "|"));
	if (ret < 0)
		goto fail;
	ret = ec_tk_expr_add_post_op(expr, ec_tk_str(NULL, "+"));
	if (ret < 0)
		goto fail;
	ret = ec_tk_expr_add_post_op(expr, ec_tk_str(NULL, "*"));
	if (ret < 0)
		goto fail;
	ret = ec_tk_expr_add_parenthesis(expr, ec_tk_str(NULL, "["),
		ec_tk_str(NULL, "]"));
	if (ret < 0)
		goto fail;
	ec_tk_expr_add_parenthesis(expr, ec_tk_str(NULL, "("),
		ec_tk_str(NULL, ")"));
	if (ret < 0)
		goto fail;

	/* prepend a lexer and a "many" to the expression token */
	ret = -ENOMEM;
	lex = ec_tk_re_lex(NULL,
		ec_tk_many(NULL, ec_tk_clone(expr), 1, 0));
	if (lex == NULL)
		goto fail;

	ret = ec_tk_re_lex_add(lex, "[a-zA-Z0-9]+", 1);
	if (ret < 0)
		goto fail;
	ret = ec_tk_re_lex_add(lex, "[*|,()]", 1);
	if (ret < 0)
		goto fail;
	ret = ec_tk_re_lex_add(lex, "\\[", 1);
	if (ret < 0)
		goto fail;
	ret = ec_tk_re_lex_add(lex, "\\]", 1);
	if (ret < 0)
		goto fail;
	ret = ec_tk_re_lex_add(lex, "[	 ]+", 0);
	if (ret < 0)
		goto fail;

	/* parse the command expression */
	ret = -ENOMEM;
	p = ec_tk_parse(lex, tk->cmd_str);
	if (p == NULL)
		goto fail;

	ret = -EINVAL;
	if (!ec_parsed_tk_matches(p))
		goto fail;
	if (TAILQ_EMPTY(&p->children))
		goto fail;
	if (TAILQ_EMPTY(&TAILQ_FIRST(&p->children)->children))
		goto fail;

	ret = -ENOMEM;
	cmd = ec_tk_seq(NULL);
	if (cmd == NULL)
		goto fail;

	TAILQ_FOREACH(child, &TAILQ_FIRST(&p->children)->children, next) {
		ret = ec_tk_expr_eval(&result, expr, child,
			&test_ops, tk);
		if (ret < 0)
			goto fail;
		ret = ec_tk_seq_add(cmd, result);
		if (ret < 0)
			goto fail;
	}
	ec_parsed_tk_free(p);
	ec_tk_dump(stdout, cmd);

	ec_tk_free(tk->expr);
	tk->expr = expr;
	ec_tk_free(tk->lex);
	tk->lex = lex;
	ec_tk_free(tk->cmd);
	tk->cmd = cmd;

	return 0;

fail:
	ec_tk_free(expr);
	ec_tk_free(lex);
	ec_tk_free(cmd);
	return ret;
}

static struct ec_tk_type ec_tk_cmd_type = {
	.name = "cmd",
	.build = ec_tk_cmd_build,
	.parse = ec_tk_cmd_parse,
	.complete = ec_tk_cmd_complete,
	.free_priv = ec_tk_cmd_free_priv,
};

EC_TK_TYPE_REGISTER(ec_tk_cmd_type);

int ec_tk_cmd_add_child(struct ec_tk *gen_tk, struct ec_tk *child)
{
	struct ec_tk_cmd *tk = (struct ec_tk_cmd *)gen_tk;
	struct ec_tk **table;

	// XXX check tk type

	assert(tk != NULL);

	printf("add child %s\n", child->id);
	if (child == NULL)
		return -EINVAL;

	gen_tk->flags &= ~EC_TK_F_BUILT;

	table = ec_realloc(tk->table, (tk->len + 1) * sizeof(*tk->table));
	if (table == NULL) {
		ec_tk_free(child);
		return -ENOMEM;
	}

	tk->table = table;
	table[tk->len] = child;
	tk->len++;

	child->parent = gen_tk;
	TAILQ_INSERT_TAIL(&gen_tk->children, child, next); // XXX really needed?

	return 0;
}

struct ec_tk *ec_tk_cmd(const char *id, const char *cmd_str)
{
	struct ec_tk *gen_tk = NULL;
	struct ec_tk_cmd *tk = NULL;

	gen_tk = ec_tk_new(id, &ec_tk_cmd_type, sizeof(*tk));
	if (gen_tk == NULL)
		goto fail;

	tk = (struct ec_tk_cmd *)gen_tk;
	tk->cmd_str = ec_strdup(cmd_str);
	if (tk->cmd_str == NULL)
		goto fail;

	return gen_tk;

fail:
	ec_tk_free(gen_tk);
	return NULL;
}

struct ec_tk *__ec_tk_cmd(const char *id, const char *cmd, ...)
{
	struct ec_tk *gen_tk = NULL;
	struct ec_tk_cmd *tk = NULL;
	struct ec_tk *child;
	va_list ap;
	int fail = 0;

	va_start(ap, cmd);

	gen_tk = ec_tk_cmd(id, cmd);
	tk = (struct ec_tk_cmd *)gen_tk;
	if (tk == NULL)
		fail = 1;;

	for (child = va_arg(ap, struct ec_tk *);
	     child != EC_TK_ENDLIST;
	     child = va_arg(ap, struct ec_tk *)) {

		/* on error, don't quit the loop to avoid leaks */
		if (fail == 1 || child == NULL ||
				ec_tk_cmd_add_child(&tk->gen, child) < 0) {
			fail = 1;
			ec_tk_free(child);
		}
	}

	if (fail == 1)
		goto fail;

	va_end(ap);
	return gen_tk;

fail:
	ec_tk_free(gen_tk); /* will also free children */
	va_end(ap);
	return NULL;
}

static int ec_tk_cmd_testcase(void)
{
	struct ec_tk *tk;
	int ret = 0;

	tk = EC_TK_CMD(NULL,
		"add [toto] x | y",
		ec_tk_int("x", 0, 10, 10),
		ec_tk_int("y", 20, 30, 10)
	);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 2, "add", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 2, "add", "23");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 3, "add", "toto", "23");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "add", "15");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "foo");
	ec_tk_free(tk);

	// XXX completion

	return ret;
}

static struct ec_test ec_tk_cmd_test = {
	.name = "tk_cmd",
	.test = ec_tk_cmd_testcase,
};

EC_TEST_REGISTER(ec_tk_cmd_test);
