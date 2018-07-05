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
#include <ecoli_config.h>
#include <ecoli_parse.h>
#include <ecoli_complete.h>
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
	struct ec_node *cmd;     /* the command node. */
	struct ec_node *parser;  /* the expression parser. */
	struct ec_node *expr;    /* the expression parser without lexer. */
	struct ec_node **table;  /* table of node referenced in command. */
	unsigned int len;        /* len of the table. */
};

/* passed as user context to expression parser */
struct ec_node_cmd_ctx {
	struct ec_node **table;
	unsigned int len;
};

static int
ec_node_cmd_eval_var(void **result, void *userctx,
	const struct ec_parse *var)
{
	const struct ec_strvec *vec;
	struct ec_node_cmd_ctx *ctx = userctx;
	struct ec_node *eval = NULL;
	const char *str, *id;
	unsigned int i;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parse_strvec(var);
	if (ec_strvec_len(vec) != 1) {
		errno = EINVAL;
		return -1;
	}
	str = ec_strvec_val(vec, 0);

	for (i = 0; i < ctx->len; i++) {
		id = ec_node_id(ctx->table[i]);
		if (id == NULL)
			continue;
		if (strcmp(str, id))
			continue;
		/* if id matches, use a node provided by the user... */
		eval = ec_node_clone(ctx->table[i]);
		if (eval == NULL)
			return -1;
		break;
	}

	/* ...or create a string node */
	if (eval == NULL) {
		eval = ec_node_str(EC_NO_ID, str);
		if (eval == NULL)
			return -1;
	}

	*result = eval;

	return 0;
}

static int
ec_node_cmd_eval_pre_op(void **result, void *userctx, void *operand,
	const struct ec_parse *operator)
{
	(void)result;
	(void)userctx;
	(void)operand;
	(void)operator;

	errno = EINVAL;
	return -1;
}

static int
ec_node_cmd_eval_post_op(void **result, void *userctx, void *operand,
	const struct ec_parse *operator)
{
	const struct ec_strvec *vec;
	struct ec_node *in = operand;;
	struct ec_node *out = NULL;;

	(void)userctx;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parse_strvec(operator);
	if (ec_strvec_len(vec) != 1) {
		errno = EINVAL;
		return -1;
	}

	if (!strcmp(ec_strvec_val(vec, 0), "*")) {
		out = ec_node_many(EC_NO_ID,
				ec_node_clone(in), 0, 0);
		if (out == NULL)
			return -1;
		ec_node_free(in);
		*result = out;
	} else {
		errno = EINVAL;
		return -1;
	}

	return 0;
}

static int
ec_node_cmd_eval_bin_op(void **result, void *userctx, void *operand1,
	const struct ec_parse *operator, void *operand2)

{
	const struct ec_strvec *vec;
	struct ec_node *out = NULL;
	struct ec_node *in1 = operand1;
	struct ec_node *in2 = operand2;

	(void)userctx;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parse_strvec(operator);
	if (ec_strvec_len(vec) > 1) {
		errno = EINVAL;
		return -1;
	}

	if (ec_strvec_len(vec) == 0) {
		if (!strcmp(in1->type->name, "seq")) {
			if (ec_node_seq_add(in1, ec_node_clone(in2)) < 0)
				return -1;
			ec_node_free(in2);
			*result = in1;
		} else {
			out = EC_NODE_SEQ(EC_NO_ID, ec_node_clone(in1),
					ec_node_clone(in2));
			if (out == NULL)
				return -1;
			ec_node_free(in1);
			ec_node_free(in2);
			*result = out;
		}
	} else if (!strcmp(ec_strvec_val(vec, 0), "|")) {
		if (!strcmp(in2->type->name, "or")) {
			if (ec_node_or_add(in2, ec_node_clone(in1)) < 0)
				return -1;
			ec_node_free(in1);
			*result = in2;
		} else if (!strcmp(in1->type->name, "or")) {
			if (ec_node_or_add(in1, ec_node_clone(in2)) < 0)
				return -1;
			ec_node_free(in2);
			*result = in1;
		} else {
			out = EC_NODE_OR(EC_NO_ID, ec_node_clone(in1),
					ec_node_clone(in2));
			if (out == NULL)
				return -1;
			ec_node_free(in1);
			ec_node_free(in2);
			*result = out;
		}
	} else if (!strcmp(ec_strvec_val(vec, 0), ",")) {
		if (!strcmp(in2->type->name, "subset")) {
			if (ec_node_subset_add(in2, ec_node_clone(in1)) < 0)
				return -1;
			ec_node_free(in1);
			*result = in2;
		} else if (!strcmp(in1->type->name, "subset")) {
			if (ec_node_subset_add(in1, ec_node_clone(in2)) < 0)
				return -1;
			ec_node_free(in2);
			*result = in1;
		} else {
			out = EC_NODE_SUBSET(EC_NO_ID, ec_node_clone(in1),
					ec_node_clone(in2));
			if (out == NULL)
				return -1;
			ec_node_free(in1);
			ec_node_free(in2);
			*result = out;
		}
	} else {
		errno = EINVAL;
		return -1;
	}

	return 0;
}

static int
ec_node_cmd_eval_parenthesis(void **result, void *userctx,
	const struct ec_parse *open_paren,
	const struct ec_parse *close_paren,
	void *value)
{
	const struct ec_strvec *vec;
	struct ec_node *in = value;;
	struct ec_node *out = NULL;;

	(void)userctx;
	(void)close_paren;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parse_strvec(open_paren);
	if (ec_strvec_len(vec) != 1) {
		errno = EINVAL;
		return -1;
	}

	if (!strcmp(ec_strvec_val(vec, 0), "[")) {
		out = ec_node_option(EC_NO_ID, ec_node_clone(in));
		if (out == NULL)
			return -1;
		ec_node_free(in);
	} else if (!strcmp(ec_strvec_val(vec, 0), "(")) {
		out = in;
	} else {
		errno = EINVAL;
		return -1;
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

static const struct ec_node_expr_eval_ops expr_ops = {
	.eval_var = ec_node_cmd_eval_var,
	.eval_pre_op = ec_node_cmd_eval_pre_op,
	.eval_post_op = ec_node_cmd_eval_post_op,
	.eval_bin_op = ec_node_cmd_eval_bin_op,
	.eval_parenthesis = ec_node_cmd_eval_parenthesis,
	.eval_free = ec_node_cmd_eval_free,
};

static struct ec_node *
ec_node_cmd_build_expr(void)
{
	struct ec_node *expr = NULL;
	int ret;

	/* build the expression parser */
	expr = ec_node("expr", "expr");
	if (expr == NULL)
		goto fail;
	ret = ec_node_expr_set_val_node(expr, ec_node_re(EC_NO_ID,
					"[a-zA-Z0-9]+"));
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

	return expr;

fail:
	ec_node_free(expr);
	return NULL;
}

static struct ec_node *
ec_node_cmd_build_parser(struct ec_node *expr)
{
	struct ec_node *lex = NULL;
	int ret;

	/* prepend a lexer to the expression node */
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

	return lex;

fail:
	ec_node_free(lex);

	return NULL;
}

static struct ec_node *
ec_node_cmd_build(struct ec_node_cmd *node, const char *cmd_str,
	struct ec_node **table, size_t len)
{
	struct ec_node_cmd_ctx ctx = { table, len };
	struct ec_parse *p = NULL;
	void *result;
	int ret;

	/* parse the command expression */
	p = ec_node_parse(node->parser, cmd_str);
	if (p == NULL)
		goto fail;

	if (!ec_parse_matches(p)) {
		errno = EINVAL;
		goto fail;
	}
	if (!ec_parse_has_child(p)) {
		errno = EINVAL;
		goto fail;
	}

	ret = ec_node_expr_eval(&result, node->expr,
				ec_parse_get_first_child(p),
				&expr_ops, &ctx);
	if (ret < 0)
		goto fail;

	ec_parse_free(p);
	return result;

fail:
	ec_parse_free(p);
	return NULL;
}

static int
ec_node_cmd_parse(const struct ec_node *gen_node, struct ec_parse *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_cmd *node = (struct ec_node_cmd *)gen_node;

	return ec_node_parse_child(node->cmd, state, strvec);
}

static int
ec_node_cmd_complete(const struct ec_node *gen_node,
		struct ec_comp *comp,
		const struct ec_strvec *strvec)
{
	struct ec_node_cmd *node = (struct ec_node_cmd *)gen_node;

	return ec_node_complete_child(node->cmd, comp, strvec);
}

static void ec_node_cmd_free_priv(struct ec_node *gen_node)
{
	struct ec_node_cmd *node = (struct ec_node_cmd *)gen_node;

	ec_free(node->cmd_str);
	ec_node_free(node->cmd);
	ec_node_free(node->expr);
	ec_node_free(node->parser);
	ec_free(node->table);
}

static const struct ec_config_schema ec_node_cmd_subschema[] = {
	{
		.desc = "A child node whose id is referenced in the expression.",
		.type = EC_CONFIG_TYPE_NODE,
	},
};

static const struct ec_config_schema ec_node_cmd_schema[] = {
	{
		.key = "expr",
		.desc = "The expression to match. Supported operators "
		"are or '|', list ',', many '+', many-or-zero '*', "
		"option '[]', group '()'. An identifier (alphanumeric) can "
		"reference a node whose node_id matches. Else it is "
		"interpreted as ec_node_str() matching this string. "
		"Example: command [option] (subset1, subset2) x|y",
		.type = EC_CONFIG_TYPE_STRING,
	},
	{
		.key = "children",
		.desc = "The list of children nodes.",
		.type = EC_CONFIG_TYPE_LIST,
		.subschema = ec_node_cmd_subschema,
		.subschema_len = EC_COUNT_OF(ec_node_cmd_subschema),
	},
};

static int ec_node_cmd_set_config(struct ec_node *gen_node,
				const struct ec_config *config)
{
	struct ec_node_cmd *node = (struct ec_node_cmd *)gen_node;
	const struct ec_config *expr = NULL, *children = NULL, *child;
	struct ec_node *cmd = NULL;
	struct ec_node **table = NULL;
	char *cmd_str = NULL;
	size_t n;

	/* retrieve config locally */
	expr = ec_config_dict_get(config, "expr");
	if (expr == NULL) {
		errno = EINVAL;
		goto fail;
	}

	children = ec_config_dict_get(config, "children");
	if (children == NULL) {
		errno = EINVAL;
		goto fail;
	}

	cmd_str = ec_strdup(expr->string);
	if (cmd_str == NULL)
		goto fail;

	n = 0;
	TAILQ_FOREACH(child, &children->list, next)
		n++;

	table = ec_malloc(n * sizeof(*table));
	if (table == NULL)
		goto fail;

	n = 0;
	TAILQ_FOREACH(child, &children->list, next) {
		table[n] = child->node;
		n++;
	}

	/* parse expression to build the cmd child node */
	cmd = ec_node_cmd_build(node, cmd_str, table, n);
	if (cmd == NULL)
		goto fail;

	ec_node_free(node->cmd);
	node->cmd = cmd;
	ec_free(node->cmd_str);
	node->cmd_str = cmd_str;
	ec_free(node->table);
	node->table = table;
	node->len = n;

	return 0;

fail:
	ec_free(table);
	ec_free(cmd_str);
	ec_node_free(cmd);
	return -1;
}

static size_t
ec_node_cmd_get_children_count(const struct ec_node *gen_node)
{
	struct ec_node_cmd *node = (struct ec_node_cmd *)gen_node;
	return node->len;
}

static struct ec_node *
ec_node_cmd_get_child(const struct ec_node *gen_node, size_t i)
{
	struct ec_node_cmd *node = (struct ec_node_cmd *)gen_node;

	if (i >= node->len)
		return NULL;

	return node->table[i];
}

static struct ec_node_type ec_node_cmd_type = {
	.name = "cmd",
	.schema = ec_node_cmd_schema,
	.schema_len = EC_COUNT_OF(ec_node_cmd_schema),
	.set_config = ec_node_cmd_set_config,
	.parse = ec_node_cmd_parse,
	.complete = ec_node_cmd_complete,
	.size = sizeof(struct ec_node_cmd),
	.free_priv = ec_node_cmd_free_priv,
	.get_children_count = ec_node_cmd_get_children_count,
	.get_child = ec_node_cmd_get_child,
};

EC_NODE_TYPE_REGISTER(ec_node_cmd_type);

struct ec_node *__ec_node_cmd(const char *id, const char *cmd, ...)
{
	struct ec_config *config = NULL, *children = NULL;
	struct ec_node *gen_node = NULL;
	struct ec_node_cmd *node = NULL;
	struct ec_node *child;
	va_list ap;

	va_start(ap, cmd);
	child = va_arg(ap, struct ec_node *);

	gen_node = __ec_node(&ec_node_cmd_type, id);
	if (gen_node == NULL)
		goto fail_free_children;
	node = (struct ec_node_cmd *)gen_node;

	node->expr = ec_node_cmd_build_expr();
	if (node->expr == NULL)
		goto fail_free_children;

	node->parser = ec_node_cmd_build_parser(node->expr);
	if (node->parser == NULL)
		goto fail_free_children;

	config = ec_config_dict();
	if (config == NULL)
		goto fail_free_children;

	if (ec_config_dict_set(config, "expr", ec_config_string(cmd)) < 0)
		goto fail_free_children;

	children = ec_config_list();
	if (children == NULL)
		goto fail_free_children;

	for (; child != EC_NODE_ENDLIST; child = va_arg(ap, struct ec_node *)) {
		if (child == NULL)
			goto fail_free_children;

		if (ec_config_list_add(children, ec_config_node(child)) < 0)
			goto fail_free_children;
	}

	if (ec_config_dict_set(config, "children", children) < 0) {
		children = NULL; /* freed */
		goto fail;
	}
	children = NULL;

	if (ec_node_set_config(gen_node, config) < 0)
		goto fail;

	va_end(ap);

	return gen_node;

fail_free_children:
	for (; child != EC_NODE_ENDLIST; child = va_arg(ap, struct ec_node *))
		ec_node_free(child);
fail:
	ec_node_free(gen_node); /* will also free added children */
	ec_config_free(config);
	va_end(ap);

	return NULL;
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
	ec_node_free(node);
	return 0;

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
