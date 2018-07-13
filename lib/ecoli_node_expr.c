/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_parse.h>
#include <ecoli_complete.h>
#include <ecoli_node_seq.h>
#include <ecoli_node_many.h>
#include <ecoli_node_or.h>
#include <ecoli_node_expr.h>

EC_LOG_TYPE_REGISTER(node_expr);

struct ec_node_expr {
	struct ec_node gen;

	/* the built node */
	struct ec_node *child;

	/* the configuration nodes */
	struct ec_node *val_node;
	struct ec_node **bin_ops;
	unsigned int bin_ops_len;
	struct ec_node **pre_ops;
	unsigned int pre_ops_len;
	struct ec_node **post_ops;
	unsigned int post_ops_len;
	struct ec_node **open_ops;
	struct ec_node **close_ops;
	unsigned int paren_len;
};

static int ec_node_expr_parse(const struct ec_node *gen_node,
			struct ec_parse *state,
			const struct ec_strvec *strvec)
{
	struct ec_node_expr *node = (struct ec_node_expr *)gen_node;

	if (node->child == NULL) {
		errno = ENOENT;
		return -1;
	}

	return ec_node_parse_child(node->child, state, strvec);
}

static int
ec_node_expr_complete(const struct ec_node *gen_node,
		struct ec_comp *comp,
		const struct ec_strvec *strvec)
{
	struct ec_node_expr *node = (struct ec_node_expr *)gen_node;

	if (node->child == NULL) {
		errno = ENOENT;
		return -1;
	}

	return ec_node_complete_child(node->child, comp, strvec);
}

static void ec_node_expr_free_priv(struct ec_node *gen_node)
{
	struct ec_node_expr *node = (struct ec_node_expr *)gen_node;
	unsigned int i;

	EC_LOG(EC_LOG_DEBUG, "free %p %p %p\n", node, node->child, node->val_node);
	ec_node_free(node->val_node);

	for (i = 0; i < node->bin_ops_len; i++)
		ec_node_free(node->bin_ops[i]);
	ec_free(node->bin_ops);
	for (i = 0; i < node->pre_ops_len; i++)
		ec_node_free(node->pre_ops[i]);
	ec_free(node->pre_ops);
	for (i = 0; i < node->post_ops_len; i++)
		ec_node_free(node->post_ops[i]);
	ec_free(node->post_ops);
	for (i = 0; i < node->paren_len; i++) {
		ec_node_free(node->open_ops[i]);
		ec_node_free(node->close_ops[i]);
	}
	ec_free(node->open_ops);
	ec_free(node->close_ops);

	ec_node_free(node->child);
}

static int ec_node_expr_build(struct ec_node_expr *node)
{
	struct ec_node *term = NULL, *expr = NULL, *next = NULL,
		*pre_op = NULL, *post_op = NULL, *ref = NULL,
		*post = NULL;
	unsigned int i;

	ec_node_free(node->child);
	node->child = NULL;

	if (node->val_node == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (node->bin_ops_len == 0 && node->pre_ops_len == 0 &&
			node->post_ops_len == 0) {
		errno = EINVAL;
		return -1;
	}

	/*
	 * Example of created grammar:
	 *
	 * pre_op = "!"
	 * post_op = "^"
	 * post = val |
	 *        pre_op expr |
	 *        "(" expr ")"
	 * term = post post_op*
	 * prod = term ( "*" term )*
	 * sum = prod ( "+" prod )*
	 * expr = sum
	 */

	/* we use this as a ref, will be set later */
	ref = ec_node("seq", "ref");
	if (ref == NULL)
		return -1;

	/* prefix unary operators */
	pre_op = ec_node("or", "pre-op");
	if (pre_op == NULL)
		goto fail;
	for (i = 0; i < node->pre_ops_len; i++) {
		if (ec_node_or_add(pre_op, ec_node_clone(node->pre_ops[i])) < 0)
			goto fail;
	}

	/* suffix unary operators */
	post_op = ec_node("or", "post-op");
	if (post_op == NULL)
		goto fail;
	for (i = 0; i < node->post_ops_len; i++) {
		if (ec_node_or_add(post_op, ec_node_clone(node->post_ops[i])) < 0)
			goto fail;
	}

	post = ec_node("or", "post");
	if (post == NULL)
		goto fail;
	if (ec_node_or_add(post, ec_node_clone(node->val_node)) < 0)
		goto fail;
	if (ec_node_or_add(post,
		EC_NODE_SEQ(EC_NO_ID,
			ec_node_clone(pre_op),
			ec_node_clone(ref))) < 0)
		goto fail;
	for (i = 0; i < node->paren_len; i++) {
		if (ec_node_or_add(post, EC_NODE_SEQ(EC_NO_ID,
					ec_node_clone(node->open_ops[i]),
					ec_node_clone(ref),
					ec_node_clone(node->close_ops[i]))) < 0)
			goto fail;
	}
	term = EC_NODE_SEQ("term",
		ec_node_clone(post),
		ec_node_many(EC_NO_ID, ec_node_clone(post_op), 0, 0)
	);
	if (term == NULL)
		goto fail;

	for (i = 0; i < node->bin_ops_len; i++) {
		next = EC_NODE_SEQ("next",
			ec_node_clone(term),
			ec_node_many(EC_NO_ID,
				EC_NODE_SEQ(EC_NO_ID,
					ec_node_clone(node->bin_ops[i]),
					ec_node_clone(term)
				),
				0, 0
			)
		);
		ec_node_free(term);
		term = next;
		if (term == NULL)
			goto fail;
	}
	expr = term;
	term = NULL;

	/* free the initial references */
	ec_node_free(pre_op);
	pre_op = NULL;
	ec_node_free(post_op);
	post_op = NULL;
	ec_node_free(post);
	post = NULL;

	if (ec_node_seq_add(ref, ec_node_clone(expr)) < 0)
		goto fail;
	ec_node_free(ref);
	ref = NULL;

	node->child = expr;

	return 0;

fail:
	ec_node_free(term);
	ec_node_free(expr);
	ec_node_free(pre_op);
	ec_node_free(post_op);
	ec_node_free(post);
	ec_node_free(ref);

	return -1;
}

static struct ec_node_type ec_node_expr_type = {
	.name = "expr",
	.parse = ec_node_expr_parse,
	.complete = ec_node_expr_complete,
	.size = sizeof(struct ec_node_expr),
	.free_priv = ec_node_expr_free_priv,
};

EC_NODE_TYPE_REGISTER(ec_node_expr_type);

int ec_node_expr_set_val_node(struct ec_node *gen_node, struct ec_node *val_node)
{
	struct ec_node_expr *node = (struct ec_node_expr *)gen_node;

	if (ec_node_check_type(gen_node, &ec_node_expr_type) < 0)
		goto fail;

	if (val_node == NULL) {
		errno = EINVAL;
		goto fail;
	}

	ec_node_free(node->val_node);
	node->val_node = val_node;
	ec_node_expr_build(node);

	return 0;

fail:
	ec_node_free(val_node);
	return -1;
}

/* add a binary operator */
int ec_node_expr_add_bin_op(struct ec_node *gen_node, struct ec_node *op)
{
	struct ec_node_expr *node = (struct ec_node_expr *)gen_node;
	struct ec_node **bin_ops;

	if (ec_node_check_type(gen_node, &ec_node_expr_type) < 0)
		goto fail;

	if (node == NULL || op == NULL) {
		errno = EINVAL;
		goto fail;
	}

	bin_ops = ec_realloc(node->bin_ops,
		(node->bin_ops_len + 1) * sizeof(*node->bin_ops));
	if (bin_ops == NULL)
		goto fail;;

	node->bin_ops = bin_ops;
	bin_ops[node->bin_ops_len] = op;
	node->bin_ops_len++;
	ec_node_expr_build(node);

	return 0;

fail:
	ec_node_free(op);
	return -1;
}

/* add a unary pre-operator */
int ec_node_expr_add_pre_op(struct ec_node *gen_node, struct ec_node *op)
{
	struct ec_node_expr *node = (struct ec_node_expr *)gen_node;
	struct ec_node **pre_ops;

	if (ec_node_check_type(gen_node, &ec_node_expr_type) < 0)
		goto fail;

	if (node == NULL || op == NULL) {
		errno = EINVAL;
		goto fail;
	}

	pre_ops = ec_realloc(node->pre_ops,
		(node->pre_ops_len + 1) * sizeof(*node->pre_ops));
	if (pre_ops == NULL)
		goto fail;

	node->pre_ops = pre_ops;
	pre_ops[node->pre_ops_len] = op;
	node->pre_ops_len++;
	ec_node_expr_build(node);

	return 0;

fail:
	ec_node_free(op);
	return -1;
}

/* add a unary post-operator */
int ec_node_expr_add_post_op(struct ec_node *gen_node, struct ec_node *op)
{
	struct ec_node_expr *node = (struct ec_node_expr *)gen_node;
	struct ec_node **post_ops;

	if (ec_node_check_type(gen_node, &ec_node_expr_type) < 0)
		goto fail;

	if (node == NULL || op == NULL) {
		errno = EINVAL;
		goto fail;
	}

	post_ops = ec_realloc(node->post_ops,
		(node->post_ops_len + 1) * sizeof(*node->post_ops));
	if (post_ops == NULL)
		goto fail;

	node->post_ops = post_ops;
	post_ops[node->post_ops_len] = op;
	node->post_ops_len++;
	ec_node_expr_build(node);

	return 0;

fail:
	ec_node_free(op);
	return -1;
}

/* add parenthesis symbols */
int ec_node_expr_add_parenthesis(struct ec_node *gen_node,
	struct ec_node *open, struct ec_node *close)
{
	struct ec_node_expr *node = (struct ec_node_expr *)gen_node;
	struct ec_node **open_ops, **close_ops;

	if (ec_node_check_type(gen_node, &ec_node_expr_type) < 0)
		goto fail;

	if (node == NULL || open == NULL || close == NULL) {
		errno = EINVAL;
		goto fail;
	}

	open_ops = ec_realloc(node->open_ops,
		(node->paren_len + 1) * sizeof(*node->open_ops));
	if (open_ops == NULL)
		goto fail;
	close_ops = ec_realloc(node->close_ops,
		(node->paren_len + 1) * sizeof(*node->close_ops));
	if (close_ops == NULL)
		goto fail;

	node->open_ops = open_ops;
	node->close_ops = close_ops;
	open_ops[node->paren_len] = open;
	close_ops[node->paren_len] = close;
	node->paren_len++;
	ec_node_expr_build(node);

	return 0;

fail:
	ec_node_free(open);
	ec_node_free(close);
	return -1;
}

enum expr_node_type {
	NONE,
	VAL,
	BIN_OP,
	PRE_OP,
	POST_OP,
	PAREN_OPEN,
	PAREN_CLOSE,
};

static enum expr_node_type get_node_type(const struct ec_node *expr_gen_node,
	const struct ec_node *check)
{
	struct ec_node_expr *expr_node = (struct ec_node_expr *)expr_gen_node;
	size_t i;

	if (check == expr_node->val_node)
		return VAL;

	for (i = 0; i < expr_node->bin_ops_len; i++) {
		if (check == expr_node->bin_ops[i])
			return BIN_OP;
	}
	for (i = 0; i < expr_node->pre_ops_len; i++) {
		if (check == expr_node->pre_ops[i])
			return PRE_OP;
	}
	for (i = 0; i < expr_node->post_ops_len; i++) {
		if (check == expr_node->post_ops[i])
			return POST_OP;
	}

	for (i = 0; i < expr_node->paren_len; i++) {
		if (check == expr_node->open_ops[i])
			return PAREN_OPEN;
	}
	for (i = 0; i < expr_node->paren_len; i++) {
		if (check == expr_node->close_ops[i])
			return PAREN_CLOSE;
	}

	return NONE;
}

struct result {
	bool has_val;
	void *val;
	const struct ec_parse *op;
	enum expr_node_type op_type;
};

/* merge x and y results in x */
static int merge_results(void *userctx,
	const struct ec_node_expr_eval_ops *ops,
	struct result *x, const struct result *y)
{
	if (y->has_val == 0 && y->op == NULL)
		return 0;
	if (x->has_val == 0 && x->op == NULL) {
		*x = *y;
		return 0;
	}

	if (x->has_val && y->has_val && y->op != NULL) {
		if (y->op_type == BIN_OP) {
			if (ops->eval_bin_op(&x->val, userctx, x->val,
					y->op, y->val) < 0)
				return -1;

			return 0;
		}
	}

	if (x->has_val == 0 && x->op != NULL && y->has_val && y->op == NULL) {
		if (x->op_type == PRE_OP) {
			if (ops->eval_pre_op(&x->val, userctx, y->val,
						x->op) < 0)
				return -1;
			x->has_val = true;
			x->op_type = NONE;
			x->op = NULL;
			return 0;
		} else if (x->op_type == BIN_OP) {
			x->val = y->val;
			x->has_val = true;
			return 0;
		}
	}

	if (x->has_val && x->op == NULL && y->has_val == 0 && y->op != NULL) {
		if (ops->eval_post_op(&x->val, userctx, x->val, y->op) < 0)
			return -1;

		return 0;
	}

	assert(false); /* we should not get here */
	return -1;
}

static int eval_expression(struct result *result,
	void *userctx,
	const struct ec_node_expr_eval_ops *ops,
	const struct ec_node *expr_gen_node,
	const struct ec_parse *parse)

{
	struct ec_parse *open = NULL, *close = NULL;
	struct result child_result;
	struct ec_parse *child;
	enum expr_node_type type;

	memset(result, 0, sizeof(*result));
	memset(&child_result, 0, sizeof(child_result));

	type = get_node_type(expr_gen_node, ec_parse_get_node(parse));
	if (type == VAL) {
		if (ops->eval_var(&result->val, userctx, parse) < 0)
			goto fail;
		result->has_val = 1;
	} else if (type == PRE_OP || type == POST_OP || type == BIN_OP) {
		result->op = parse;
		result->op_type = type;
	}

	EC_PARSE_FOREACH_CHILD(child, parse) {

		type = get_node_type(expr_gen_node, ec_parse_get_node(child));
		if (type == PAREN_OPEN) {
			open = child;
			continue;
		} else if (type == PAREN_CLOSE) {
			close = child;
			continue;
		}

		if (eval_expression(&child_result, userctx, ops,
			expr_gen_node, child) < 0)
			goto fail;

		if (merge_results(userctx, ops, result, &child_result) < 0)
			goto fail;

		memset(&child_result, 0, sizeof(child_result));
	}

	if (open != NULL && close != NULL) {
		if (ops->eval_parenthesis(&result->val, userctx, open, close,
			result->val) < 0)
			goto fail;
	}

	return 0;

fail:
	if (result->has_val)
		ops->eval_free(result->val, userctx);
	if (child_result.has_val)
		ops->eval_free(child_result.val, userctx);
	memset(result, 0, sizeof(*result));

	return -1;
}

int ec_node_expr_eval(void **user_result, const struct ec_node *node,
	struct ec_parse *parse, const struct ec_node_expr_eval_ops *ops,
	void *userctx)
{
	struct result result;

	if (ops == NULL || ops->eval_var == NULL || ops->eval_pre_op == NULL ||
			ops->eval_post_op == NULL || ops->eval_bin_op == NULL ||
			ops->eval_parenthesis == NULL ||
			ops->eval_free == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (ec_node_check_type(node, &ec_node_expr_type) < 0)
		return -1;

	if (!ec_parse_matches(parse)) {
		errno = EINVAL;
		return -1;
	}

	if (eval_expression(&result, userctx, ops, node, parse) < 0)
		return -1;

	assert(result.has_val);
	assert(result.op == NULL);
	*user_result = result.val;

	return 0;
}

/* the test case is in a separate file ecoli_node_expr_test.c */
