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
#include <ecoli_node.h>
#include <ecoli_parsed.h>
#include <ecoli_completed.h>
#include <ecoli_node_expr.h>
#include <ecoli_node_str.h>
#include <ecoli_node_or.h>
#include <ecoli_node_subset.h>
#include <ecoli_node_int.h>
#include <ecoli_node_many.h>
#include <ecoli_node_seq.h>
#include <ecoli_node_option.h>
#include <ecoli_node_re.h>
#include <ecoli_node_re_lex.h>
#include <ecoli_node_cmd.h>

EC_LOG_TYPE_REGISTER(node_cmd);

struct ec_node_cmd {
	struct ec_node gen;
	char *cmd_str;           /* the command string. */
	struct ec_node *cmd;       /* the command node. */
	struct ec_node *lex;       /* the lexer node. */
	struct ec_node *expr;      /* the expression parser. */
	struct ec_node **table;    /* table of node referenced in command. */
	unsigned int len;        /* len of the table. */
};

static int
ec_node_cmd_eval_var(void **result, void *userctx,
	const struct ec_parsed *var)
{
	const struct ec_strvec *vec;
	struct ec_node_cmd *node = userctx;
	struct ec_node *eval = NULL;
	const char *str, *id;
	unsigned int i;

	(void)userctx;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parsed_strvec(var);
	if (ec_strvec_len(vec) != 1)
		return -EINVAL;
	str = ec_strvec_val(vec, 0);

	for (i = 0; i < node->len; i++) {
		id = ec_node_id(node->table[i]);
		printf("i=%d id=%s\n", i, id);
		if (id == NULL)
			continue;
		if (strcmp(str, id))
			continue;
		/* if id matches, use a node provided by the user... */
		eval = ec_node_clone(node->table[i]);
		if (eval == NULL)
			return -ENOMEM;
		break;
	}

	/* ...or create a string node */
	if (eval == NULL) {
		eval = ec_node_str(NULL, str);
		if (eval == NULL)
			return -ENOMEM;
	}

	printf("eval var %s %p\n", str, eval);
	*result = eval;

	return 0;
}

static int
ec_node_cmd_eval_pre_op(void **result, void *userctx, void *operand,
	const struct ec_parsed *operator)
{
	(void)result;
	(void)userctx;
	(void)operand;
	(void)operator;

	return -EINVAL;
}

static int
ec_node_cmd_eval_post_op(void **result, void *userctx, void *operand,
	const struct ec_parsed *operator)
{
	const struct ec_strvec *vec;
	struct ec_node *eval = operand;;

	(void)userctx;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parsed_strvec(operator);
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
ec_node_cmd_eval_bin_op(void **result, void *userctx, void *operand1,
	const struct ec_parsed *operator, void *operand2)

{
	const struct ec_strvec *vec;
	struct ec_node *out = NULL;
	struct ec_node *in1 = operand1;
	struct ec_node *in2 = operand2;

	(void)userctx;

	printf("eval bin_op %p %p\n", in1, in2);

	/* get parsed string vector, it should contain only one str */
	vec = ec_parsed_strvec(operator);
	if (ec_strvec_len(vec) != 1)
		return -EINVAL;

	if (!strcmp(ec_strvec_val(vec, 0), "|")) {
		out = EC_NODE_OR(NULL, ec_node_clone(in1), ec_node_clone(in2));
		if (out == NULL)
			return -EINVAL;
		ec_node_free(in1);
		ec_node_free(in2);
		*result = out;
	} else if (!strcmp(ec_strvec_val(vec, 0), ",")) {
		if (!strcmp(in2->type->name, "subset")) {
			if (ec_node_subset_add(in2, ec_node_clone(in1)) < 0)
				return -EINVAL;
			ec_node_free(in1);
			*result = in2;
		} else {
			out = EC_NODE_SUBSET(NULL, ec_node_clone(in1),
					ec_node_clone(in2));
			if (out == NULL)
				return -EINVAL;
			ec_node_free(in1);
			ec_node_free(in2);
			*result = out;
		}
	} else {
		return -EINVAL;
	}

	printf("eval bin_op out %p\n", *result);

	return 0;
}

static int
ec_node_cmd_eval_parenthesis(void **result, void *userctx,
	const struct ec_parsed *open_paren,
	const struct ec_parsed *close_paren,
	void *value)
{
	const struct ec_strvec *vec;
	struct ec_node *in = value;;
	struct ec_node *out = NULL;;

	(void)userctx;
	(void)close_paren;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parsed_strvec(open_paren);
	if (ec_strvec_len(vec) != 1)
		return -EINVAL;

	if (!strcmp(ec_strvec_val(vec, 0), "[")) {
		out = ec_node_option(NULL, ec_node_clone(in));
		if (out == NULL)
			return -EINVAL;
		ec_node_free(in);
	} else if (!strcmp(ec_strvec_val(vec, 0), "(")) {
		out = in;
	} else {
		return -EINVAL;
	}

	printf("eval paren\n");
	*result = out;

	return 0;
}

static void
ec_node_cmd_eval_free(void *result, void *userctx)
{
	(void)userctx;
	ec_free(result);
}

static const struct ec_node_expr_eval_ops test_ops = {
	.eval_var = ec_node_cmd_eval_var,
	.eval_pre_op = ec_node_cmd_eval_pre_op,
	.eval_post_op = ec_node_cmd_eval_post_op,
	.eval_bin_op = ec_node_cmd_eval_bin_op,
	.eval_parenthesis = ec_node_cmd_eval_parenthesis,
	.eval_free = ec_node_cmd_eval_free,
};

static int
ec_node_cmd_parse(const struct ec_node *gen_node, struct ec_parsed *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_cmd *node = (struct ec_node_cmd *)gen_node;

	return ec_node_parse_child(node->cmd, state, strvec);
}

static int
ec_node_cmd_complete(const struct ec_node *gen_node,
		struct ec_completed *completed,
		struct ec_parsed *parsed,
		const struct ec_strvec *strvec)
{
	struct ec_node_cmd *node = (struct ec_node_cmd *)gen_node;

	return ec_node_complete_child(node->cmd, completed, parsed, strvec);
}

static void ec_node_cmd_free_priv(struct ec_node *gen_node)
{
	struct ec_node_cmd *node = (struct ec_node_cmd *)gen_node;
	unsigned int i;

	ec_free(node->cmd_str);
	ec_node_free(node->cmd);
	ec_node_free(node->expr);
	ec_node_free(node->lex);
	for (i = 0; i < node->len; i++)
		ec_node_free(node->table[i]);
	ec_free(node->table);
}

static int ec_node_cmd_build(struct ec_node *gen_node)
{
	struct ec_node *expr = NULL, *lex = NULL, *cmd = NULL;
	struct ec_parsed *p, *child;
	struct ec_node_cmd *node = (struct ec_node_cmd *)gen_node;
	void *result;
	int ret;

	/* XXX the expr parser can be moved in the node init */

	/* build the expression parser */
	ret = -ENOMEM;
	expr = ec_node("expr", "expr");
	if (expr == NULL)
		goto fail;
	ret = ec_node_expr_set_val_node(expr, ec_node_re(NULL, "[a-zA-Z0-9]+"));
	if (ret < 0)
		goto fail;
	ret = ec_node_expr_add_bin_op(expr, ec_node_str(NULL, ","));
	if (ret < 0)
		goto fail;
	ret = ec_node_expr_add_bin_op(expr, ec_node_str(NULL, "|"));
	if (ret < 0)
		goto fail;
	ret = ec_node_expr_add_post_op(expr, ec_node_str(NULL, "+"));
	if (ret < 0)
		goto fail;
	ret = ec_node_expr_add_post_op(expr, ec_node_str(NULL, "*"));
	if (ret < 0)
		goto fail;
	ret = ec_node_expr_add_parenthesis(expr, ec_node_str(NULL, "["),
		ec_node_str(NULL, "]"));
	if (ret < 0)
		goto fail;
	ec_node_expr_add_parenthesis(expr, ec_node_str(NULL, "("),
		ec_node_str(NULL, ")"));
	if (ret < 0)
		goto fail;

	/* prepend a lexer and a "many" to the expression node */
	ret = -ENOMEM;
	lex = ec_node_re_lex(NULL,
		ec_node_many(NULL, ec_node_clone(expr), 1, 0));
	if (lex == NULL)
		goto fail;

	ret = ec_node_re_lex_add(lex, "[a-zA-Z0-9]+", 1);
	if (ret < 0)
		goto fail;
	ret = ec_node_re_lex_add(lex, "[*|,()]", 1);
	if (ret < 0)
		goto fail;
	ret = ec_node_re_lex_add(lex, "\\[", 1);
	if (ret < 0)
		goto fail;
	ret = ec_node_re_lex_add(lex, "\\]", 1);
	if (ret < 0)
		goto fail;
	ret = ec_node_re_lex_add(lex, "[	 ]+", 0);
	if (ret < 0)
		goto fail;

	/* parse the command expression */
	ret = -ENOMEM;
	p = ec_node_parse(lex, node->cmd_str);
	if (p == NULL)
		goto fail;

	ret = -EINVAL;
	if (!ec_parsed_matches(p))
		goto fail;
	if (TAILQ_EMPTY(&p->children))
		goto fail;
	if (TAILQ_EMPTY(&TAILQ_FIRST(&p->children)->children))
		goto fail;

	ret = -ENOMEM;
	cmd = ec_node("seq", NULL);
	if (cmd == NULL)
		goto fail;

	TAILQ_FOREACH(child, &TAILQ_FIRST(&p->children)->children, next) {
		ret = ec_node_expr_eval(&result, expr, child,
			&test_ops, node);
		if (ret < 0)
			goto fail;
		ret = ec_node_seq_add(cmd, result);
		if (ret < 0)
			goto fail;
	}
	ec_parsed_free(p);
	p = NULL;
	ec_node_dump(stdout, cmd);

	ec_node_free(node->expr);
	node->expr = expr;
	ec_node_free(node->lex);
	node->lex = lex;
	ec_node_free(node->cmd);
	node->cmd = cmd;

	return 0;

fail:
	ec_parsed_free(p);
	ec_node_free(expr);
	ec_node_free(lex);
	ec_node_free(cmd);
	return ret;
}

static struct ec_node_type ec_node_cmd_type = {
	.name = "cmd",
	.build = ec_node_cmd_build,
	.parse = ec_node_cmd_parse,
	.complete = ec_node_cmd_complete,
	.size = sizeof(struct ec_node_cmd),
	.free_priv = ec_node_cmd_free_priv,
};

EC_NODE_TYPE_REGISTER(ec_node_cmd_type);

int ec_node_cmd_add_child(struct ec_node *gen_node, struct ec_node *child)
{
	struct ec_node_cmd *node = (struct ec_node_cmd *)gen_node;
	struct ec_node **table;

	// XXX check node type

	assert(node != NULL);

	printf("add child %s\n", child->id);
	if (child == NULL)
		return -EINVAL;

	gen_node->flags &= ~EC_NODE_F_BUILT;

	table = ec_realloc(node->table, (node->len + 1) * sizeof(*node->table));
	if (table == NULL) {
		ec_node_free(child);
		return -ENOMEM;
	}

	node->table = table;
	table[node->len] = child;
	node->len++;

	child->parent = gen_node;
	TAILQ_INSERT_TAIL(&gen_node->children, child, next); // XXX really needed?

	return 0;
}

struct ec_node *ec_node_cmd(const char *id, const char *cmd_str)
{
	struct ec_node *gen_node = NULL;
	struct ec_node_cmd *node = NULL;

	gen_node = __ec_node(&ec_node_cmd_type, id);
	if (gen_node == NULL)
		goto fail;

	node = (struct ec_node_cmd *)gen_node;
	node->cmd_str = ec_strdup(cmd_str);
	if (node->cmd_str == NULL)
		goto fail;

	return gen_node;

fail:
	ec_node_free(gen_node);
	return NULL;
}

struct ec_node *__ec_node_cmd(const char *id, const char *cmd, ...)
{
	struct ec_node *gen_node = NULL;
	struct ec_node_cmd *node = NULL;
	struct ec_node *child;
	va_list ap;
	int fail = 0;

	va_start(ap, cmd);

	gen_node = ec_node_cmd(id, cmd);
	node = (struct ec_node_cmd *)gen_node;
	if (node == NULL)
		fail = 1;;

	for (child = va_arg(ap, struct ec_node *);
	     child != EC_NODE_ENDLIST;
	     child = va_arg(ap, struct ec_node *)) {

		/* on error, don't quit the loop to avoid leaks */
		if (fail == 1 || child == NULL ||
				ec_node_cmd_add_child(&node->gen, child) < 0) {
			fail = 1;
			ec_node_free(child);
		}
	}

	if (fail == 1)
		goto fail;

	va_end(ap);
	return gen_node;

fail:
	ec_node_free(gen_node); /* will also free children */
	va_end(ap);
	return NULL;
}

/* LCOV_EXCL_START */
static int ec_node_cmd_testcase(void)
{
	struct ec_node *node;
	int ret = 0;

	node = EC_NODE_CMD(NULL,
		"command [option] (subset1, subset2, subset3) x|y",
		ec_node_int("x", 0, 10, 10),
		ec_node_int("y", 20, 30, 10)
	);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_PARSE(node, 2, "command", "1");
	ret |= EC_TEST_CHECK_PARSE(node, 2, "command", "23");
	ret |= EC_TEST_CHECK_PARSE(node, 3, "command", "option", "23");
	ret |= EC_TEST_CHECK_PARSE(node, -1, "command", "15");
	ret |= EC_TEST_CHECK_PARSE(node, -1, "foo");
	ec_node_free(node);

	node = EC_NODE_CMD(NULL, "good morning [count] bob|bobby|michael",
			ec_node_int("count", 0, 10, 10));
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_PARSE(node, 4, "good", "morning", "1", "bob");

	ret |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		"good", EC_NODE_ENDLIST);
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"g", EC_NODE_ENDLIST,
		"good", EC_NODE_ENDLIST);
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"good", "morning", "", EC_NODE_ENDLIST,
		"bob", "bobby", "michael", EC_NODE_ENDLIST);

	ec_node_free(node);

	return ret;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_cmd_test = {
	.name = "node_cmd",
	.test = ec_node_cmd_testcase,
};

EC_TEST_REGISTER(ec_node_cmd_test);
