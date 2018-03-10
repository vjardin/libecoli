/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
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
		eval = ec_node_str(EC_NO_ID, str);
		if (eval == NULL)
			return -ENOMEM;
	}

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
	struct ec_node *in = operand;;
	struct ec_node *out = NULL;;

	(void)userctx;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parsed_strvec(operator);
	if (ec_strvec_len(vec) != 1)
		return -EINVAL;

	if (!strcmp(ec_strvec_val(vec, 0), "*")) {
		out = ec_node_many(EC_NO_ID,
				ec_node_clone(in), 0, 0);
		if (out == NULL)
			return -EINVAL;
		ec_node_free(in);
		*result = out;
	} else {
		return -EINVAL;
	}

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

	/* get parsed string vector, it should contain only one str */
	vec = ec_parsed_strvec(operator);
	if (ec_strvec_len(vec) > 1)
		return -EINVAL;

	if (ec_strvec_len(vec) == 0) {
		if (!strcmp(in1->type->name, "seq")) {
			if (ec_node_seq_add(in1, ec_node_clone(in2)) < 0)
				return -EINVAL;
			ec_node_free(in2);
			*result = in1;
		} else {
			out = EC_NODE_SEQ(EC_NO_ID, ec_node_clone(in1),
					ec_node_clone(in2));
			if (out == NULL)
				return -EINVAL;
			ec_node_free(in1);
			ec_node_free(in2);
			*result = out;
		}
	} else if (!strcmp(ec_strvec_val(vec, 0), "|")) {
		if (!strcmp(in2->type->name, "or")) {
			if (ec_node_or_add(in2, ec_node_clone(in1)) < 0)
				return -EINVAL;
			ec_node_free(in1);
			*result = in2;
		} else if (!strcmp(in1->type->name, "or")) {
			if (ec_node_or_add(in1, ec_node_clone(in2)) < 0)
				return -EINVAL;
			ec_node_free(in2);
			*result = in1;
		} else {
			out = EC_NODE_OR(EC_NO_ID, ec_node_clone(in1),
					ec_node_clone(in2));
			if (out == NULL)
				return -EINVAL;
			ec_node_free(in1);
			ec_node_free(in2);
			*result = out;
		}
	} else if (!strcmp(ec_strvec_val(vec, 0), ",")) {
		if (!strcmp(in2->type->name, "subset")) {
			if (ec_node_subset_add(in2, ec_node_clone(in1)) < 0)
				return -EINVAL;
			ec_node_free(in1);
			*result = in2;
		} else if (!strcmp(in1->type->name, "subset")) {
			if (ec_node_subset_add(in1, ec_node_clone(in2)) < 0)
				return -EINVAL;
			ec_node_free(in2);
			*result = in1;
		} else {
			out = EC_NODE_SUBSET(EC_NO_ID, ec_node_clone(in1),
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
		out = ec_node_option(EC_NO_ID, ec_node_clone(in));
		if (out == NULL)
			return -EINVAL;
		ec_node_free(in);
	} else if (!strcmp(ec_strvec_val(vec, 0), "(")) {
		out = in;
	} else {
		return -EINVAL;
	}

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

static int ec_node_cmd_build(struct ec_node_cmd *node)
{
	struct ec_node *expr = NULL, *lex = NULL, *cmd = NULL;
	struct ec_parsed *p = NULL;
	void *result;
	int ret;

	ec_node_free(node->expr);
	node->expr = NULL;
	ec_node_free(node->lex);
	node->lex = NULL;
	ec_node_free(node->cmd);
	node->cmd = NULL;

	/* build the expression parser */
	ret = -ENOMEM;
	expr = ec_node("expr", "expr");
	if (expr == NULL)
		goto fail;
	ret = ec_node_expr_set_val_node(expr, ec_node_re(EC_NO_ID, "[a-zA-Z0-9]+"));
	if (ret < 0)
		goto fail;
	ret = ec_node_expr_add_bin_op(expr, ec_node_str(EC_NO_ID, ","));
	if (ret < 0)
		goto fail;
	ret = ec_node_expr_add_bin_op(expr, ec_node_str(EC_NO_ID, "|"));
	if (ret < 0)
		goto fail;
	ret = ec_node_expr_add_bin_op(expr, ec_node("empty", EC_NO_ID));
	if (ret < 0)
		goto fail;
	ret = ec_node_expr_add_post_op(expr, ec_node_str(EC_NO_ID, "+"));
	if (ret < 0)
		goto fail;
	ret = ec_node_expr_add_post_op(expr, ec_node_str(EC_NO_ID, "*"));
	if (ret < 0)
		goto fail;
	ret = ec_node_expr_add_parenthesis(expr, ec_node_str(EC_NO_ID, "["),
		ec_node_str(EC_NO_ID, "]"));
	if (ret < 0)
		goto fail;
	ec_node_expr_add_parenthesis(expr, ec_node_str(EC_NO_ID, "("),
		ec_node_str(EC_NO_ID, ")"));
	if (ret < 0)
		goto fail;

	/* prepend a lexer to the expression node */
	ret = -ENOMEM;
	lex = ec_node_re_lex(EC_NO_ID, ec_node_clone(expr));
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
	if (!ec_parsed_has_child(p))
		goto fail;

	ret = ec_node_expr_eval(&result, expr, ec_parsed_get_first_child(p),
				&test_ops, node);
	if (ret < 0)
		goto fail;

	ec_parsed_free(p);
	p = NULL;

	node->expr = expr;
	node->lex = lex;
	node->cmd = result;

	return 0;

fail:
	ec_parsed_free(p);
	ec_node_free(expr);
	ec_node_free(lex);
	ec_node_free(cmd);
	return ret;
}

static int
ec_node_cmd_parse(const struct ec_node *gen_node, struct ec_parsed *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_cmd *node = (struct ec_node_cmd *)gen_node;

	if (node->cmd == NULL)
		return -ENOENT;
	return ec_node_parse_child(node->cmd, state, strvec);
}

static int
ec_node_cmd_complete(const struct ec_node *gen_node,
		struct ec_completed *completed,
		const struct ec_strvec *strvec)
{
	struct ec_node_cmd *node = (struct ec_node_cmd *)gen_node;

	if (node->cmd == NULL)
		return -ENOENT;
	return ec_node_complete_child(node->cmd, completed, strvec);
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


static struct ec_node_type ec_node_cmd_type = {
	.name = "cmd",
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
	int ret;

	assert(node != NULL);

	if (child == NULL) {
		errno = EINVAL;
		goto fail;
	}

	if (ec_node_check_type(gen_node, &ec_node_cmd_type) < 0)
		goto fail;

	if (node->cmd == NULL) {
		ret = ec_node_cmd_build(node);
		if (ret < 0)
			return ret;
	}

	table = ec_realloc(node->table, (node->len + 1) * sizeof(*node->table));
	if (table == NULL)
		goto fail;

	node->table = table;

	if (ec_node_add_child(gen_node, child) < 0)
		goto fail;

	table[node->len] = child;
	node->len++;

	return 0;

fail:
	ec_node_free(child);
	return -1;
}

struct ec_node *__ec_node_cmd(const char *id, const char *cmd, ...)
{
	struct ec_node *gen_node = NULL;
	struct ec_node_cmd *node = NULL;
	struct ec_node *child;
	va_list ap;
	int fail = 0;

	gen_node = __ec_node(&ec_node_cmd_type, id);
	if (gen_node == NULL)
		fail = 1;

	if (fail == 0) {
		node = (struct ec_node_cmd *)gen_node;
		node->cmd_str = ec_strdup(cmd);
		if (node->cmd_str == NULL)
			fail = 1;
	}

	va_start(ap, cmd);

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

	va_end(ap);

	if (fail == 1)
		goto fail;

	if (ec_node_cmd_build(node) < 0)
		goto fail;

	return gen_node;

fail:
	ec_node_free(gen_node); /* will also free children */
	return NULL;
}

struct ec_node *ec_node_cmd(const char *id, const char *cmd_str)
{
	return __ec_node_cmd(id, cmd_str, EC_NODE_ENDLIST);
}

/* LCOV_EXCL_START */
static int ec_node_cmd_testcase(void)
{
	struct ec_node *node;
	int testres = 0;

	node = EC_NODE_CMD(EC_NO_ID,
		"command [option] (subset1, subset2, subset3, subset4) x|y z*",
		ec_node_int("x", 0, 10, 10),
		ec_node_int("y", 20, 30, 10)
	);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 2, "command", "1");
	testres |= EC_TEST_CHECK_PARSE(node, 3, "command", "subset1", "1");
	testres |= EC_TEST_CHECK_PARSE(node, 4, "command", "subset3", "subset2",
				"1");
	testres |= EC_TEST_CHECK_PARSE(node, 5, "command", "subset2", "subset3",
				"subset1", "1");
	testres |= EC_TEST_CHECK_PARSE(node, 6, "command", "subset3", "subset1",
				"subset4", "subset2", "4");
	testres |= EC_TEST_CHECK_PARSE(node, 2, "command", "23");
	testres |= EC_TEST_CHECK_PARSE(node, 3, "command", "option", "23");
	testres |= EC_TEST_CHECK_PARSE(node, 5, "command", "option", "23",
				"z", "z");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "command", "15");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "foo");
	ec_node_free(node);

	node = EC_NODE_CMD(EC_NO_ID, "good morning [count] bob|bobby|michael",
			ec_node_int("count", 0, 10, 10));
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 4, "good", "morning", "1", "bob");

	testres |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		"good", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"g", EC_NODE_ENDLIST,
		"good", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"good", "morning", "", EC_NODE_ENDLIST,
		"bob", "bobby", "michael", EC_NODE_ENDLIST);

	ec_node_free(node);

	node = EC_NODE_CMD(EC_NO_ID, "[foo [bar]]");
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 0);
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 2, "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 0, "x");
	ec_node_free(node);

	return testres;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_cmd_test = {
	.name = "node_cmd",
	.test = ec_node_cmd_testcase,
};

EC_TEST_REGISTER(ec_node_cmd_test);
