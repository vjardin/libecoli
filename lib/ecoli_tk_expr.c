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
#include <ecoli_tk.h>
#include <ecoli_tk_seq.h>
#include <ecoli_tk_many.h>
#include <ecoli_tk_or.h>
#include <ecoli_tk_weakref.h>
#include <ecoli_tk_expr.h>

struct ec_tk_expr {
	struct ec_tk gen;

	/* the built node */
	struct ec_tk *child;

	/* the configuration nodes */
	struct ec_tk *val_tk;
	struct ec_tk **bin_ops;
	unsigned int bin_ops_len;
	struct ec_tk **pre_ops;
	unsigned int pre_ops_len;
	struct ec_tk **post_ops;
	unsigned int post_ops_len;
	struct ec_tk **open_ops;
	struct ec_tk **close_ops;
	unsigned int paren_len;
};

static struct ec_parsed_tk *ec_tk_expr_parse(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_expr *tk = (struct ec_tk_expr *)gen_tk;

	return ec_tk_parse_tokens(tk->child, strvec);
}

static struct ec_completed_tk *ec_tk_expr_complete(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_expr *tk = (struct ec_tk_expr *)gen_tk;

	return ec_tk_complete_tokens(tk->child, strvec);
}

static void ec_tk_expr_free_priv(struct ec_tk *gen_tk)
{
	struct ec_tk_expr *tk = (struct ec_tk_expr *)gen_tk;
	unsigned int i;

	ec_log(EC_LOG_DEBUG, "free %p %p %p\n", tk, tk->child, tk->val_tk);
	ec_tk_free(tk->val_tk);

	for (i = 0; i < tk->bin_ops_len; i++)
		ec_tk_free(tk->bin_ops[i]);
	ec_free(tk->bin_ops);
	for (i = 0; i < tk->pre_ops_len; i++)
		ec_tk_free(tk->pre_ops[i]);
	ec_free(tk->pre_ops);
	for (i = 0; i < tk->post_ops_len; i++)
		ec_tk_free(tk->post_ops[i]);
	ec_free(tk->post_ops);
	for (i = 0; i < tk->paren_len; i++) {
		ec_tk_free(tk->open_ops[i]);
		ec_tk_free(tk->close_ops[i]);
	}
	ec_free(tk->open_ops);
	ec_free(tk->close_ops);

	ec_tk_free(tk->child);
}

static int ec_tk_expr_build(struct ec_tk *gen_tk)
{
	struct ec_tk_expr *tk = (struct ec_tk_expr *)gen_tk;
	struct ec_tk *term = NULL, *expr = NULL, *next = NULL,
		*pre_op = NULL, *post_op = NULL,
		*post = NULL, *weak = NULL;
	unsigned int i;
	int ret;

	if (tk->val_tk == NULL)
		return -EINVAL;
	if (tk->bin_ops_len == 0 && tk->pre_ops_len == 0 &&
			tk->post_ops_len == 0)
		return -EINVAL;

	/* create the object, we will initialize it later: this is
	 * needed because we have a circular dependency */
	ret = -ENOMEM;
	weak = ec_tk_weakref_empty("weak");
	if (weak == NULL)
		return -1;

	/* prefix unary operators */
	pre_op = ec_tk_or("pre-op");
	if (pre_op == NULL)
		goto fail;
	for (i = 0; i < tk->pre_ops_len; i++) {
		if (ec_tk_or_add(pre_op, ec_tk_clone(tk->pre_ops[i])) < 0)
			goto fail;
	}

	/* suffix unary operators */
	post_op = ec_tk_or("post-op");
	if (post_op == NULL)
		goto fail;
	for (i = 0; i < tk->post_ops_len; i++) {
		if (ec_tk_or_add(post_op, ec_tk_clone(tk->post_ops[i])) < 0)
			goto fail;
	}

	post = ec_tk_or("post");
	if (post == NULL)
		goto fail;
	if (ec_tk_or_add(post, ec_tk_clone(tk->val_tk)) < 0)
		goto fail;
	if (ec_tk_or_add(post,
		EC_TK_SEQ(NULL,
			ec_tk_clone(pre_op),
			ec_tk_clone(weak))) < 0)
		goto fail;
	for (i = 0; i < tk->paren_len; i++) {
		if (ec_tk_or_add(post, EC_TK_SEQ(NULL,
					ec_tk_clone(tk->open_ops[i]),
					ec_tk_clone(weak),
					ec_tk_clone(tk->close_ops[i]))) < 0)
			goto fail;
	}
	term = EC_TK_SEQ("term",
		ec_tk_clone(post),
		ec_tk_many(NULL, ec_tk_clone(post_op), 0, 0)
	);
	if (term == NULL)
		goto fail;

	for (i = 0; i < tk->bin_ops_len; i++) {
		next = EC_TK_SEQ("next",
			ec_tk_clone(term),
			ec_tk_many(NULL,
				EC_TK_SEQ(NULL,
					ec_tk_clone(tk->bin_ops[i]),
					ec_tk_clone(term)
				),
				0, 0
			)
		);
		ec_tk_free(term);
		term = next;
		if (term == NULL)
			goto fail;
	}
	expr = term;
	term = NULL;

	/* free the initial references */
	ec_tk_free(pre_op);
	pre_op = NULL;
	ec_tk_free(post_op);
	post_op = NULL;
	ec_tk_free(post);
	post = NULL;

	/* no need to clone here, the node is not consumed */
	if (ec_tk_weakref_set(weak, expr) < 0)
		goto fail;
	ec_tk_free(weak);
	weak = NULL;

	tk->child = expr;

	return 0;

fail:
	ec_tk_free(term);
	ec_tk_free(expr);
	ec_tk_free(pre_op);
	ec_tk_free(post_op);
	ec_tk_free(post);
	ec_tk_free(weak);

	return ret;
}

static struct ec_tk_type ec_tk_expr_type = {
	.name = "expr",
	.build = ec_tk_expr_build,
	.parse = ec_tk_expr_parse,
	.complete = ec_tk_expr_complete,
	.free_priv = ec_tk_expr_free_priv,
};

EC_TK_TYPE_REGISTER(ec_tk_expr_type);

struct ec_tk *ec_tk_expr(const char *id)
{
	struct ec_tk_expr *tk = NULL;
	struct ec_tk *gen_tk = NULL;

	gen_tk = ec_tk_new(id, &ec_tk_expr_type, sizeof(*tk));
	if (gen_tk == NULL)
		return NULL;
	tk = (struct ec_tk_expr *)gen_tk;

	return gen_tk;
}

int ec_tk_expr_set_val_tk(struct ec_tk *gen_tk, struct ec_tk *val_tk)
{
	struct ec_tk_expr *tk = (struct ec_tk_expr *)gen_tk;
	int ret;

	ret = -EINVAL;
	if (val_tk == NULL)
		goto fail;
	ret = -EPERM;
	if (gen_tk->flags & EC_TK_F_BUILT)
		goto fail;
	ret = -EEXIST;
	if (tk->val_tk != NULL)
		goto fail;

	tk->val_tk = val_tk;
	gen_tk->flags &= ~EC_TK_F_BUILT;

	return 0;

fail:
	ec_tk_free(val_tk);
	return ret;
}

/* add a binary operator */
int ec_tk_expr_add_bin_op(struct ec_tk *gen_tk, struct ec_tk *op)
{
	struct ec_tk_expr *tk = (struct ec_tk_expr *)gen_tk;
	struct ec_tk **bin_ops;
	int ret;

	// XXX check tk type

	ret = -EINVAL;
	if (tk == NULL || op == NULL)
		goto fail;
	ret = -EPERM;
	if (gen_tk->flags & EC_TK_F_BUILT)
		goto fail;

	ret = -ENOMEM;
	bin_ops = ec_realloc(tk->bin_ops,
		(tk->bin_ops_len + 1) * sizeof(*tk->bin_ops));
	if (bin_ops == NULL)
		goto fail;;

	tk->bin_ops = bin_ops;
	bin_ops[tk->bin_ops_len] = op;
	tk->bin_ops_len++;
	gen_tk->flags &= ~EC_TK_F_BUILT;

	return 0;

fail:
	ec_tk_free(op);
	return ret;
}

/* add a unary pre-operator */
int ec_tk_expr_add_pre_op(struct ec_tk *gen_tk, struct ec_tk *op)
{
	struct ec_tk_expr *tk = (struct ec_tk_expr *)gen_tk;
	struct ec_tk **pre_ops;
	int ret;

	// XXX check tk type

	ret = -EINVAL;
	if (tk == NULL || op == NULL)
		goto fail;
	ret = -EPERM;
	if (gen_tk->flags & EC_TK_F_BUILT)
		goto fail;

	ret = -ENOMEM;
	pre_ops = ec_realloc(tk->pre_ops,
		(tk->pre_ops_len + 1) * sizeof(*tk->pre_ops));
	if (pre_ops == NULL)
		goto fail;

	tk->pre_ops = pre_ops;
	pre_ops[tk->pre_ops_len] = op;
	tk->pre_ops_len++;
	gen_tk->flags &= ~EC_TK_F_BUILT;

	return 0;

fail:
	ec_tk_free(op);
	return ret;
}

/* add a unary post-operator */
int ec_tk_expr_add_post_op(struct ec_tk *gen_tk, struct ec_tk *op)
{
	struct ec_tk_expr *tk = (struct ec_tk_expr *)gen_tk;
	struct ec_tk **post_ops;
	int ret;

	// XXX check tk type

	ret = -EINVAL;
	if (tk == NULL || op == NULL)
		goto fail;
	ret = -EPERM;
	if (gen_tk->flags & EC_TK_F_BUILT)
		goto fail;

	ret = -ENOMEM;
	post_ops = ec_realloc(tk->post_ops,
		(tk->post_ops_len + 1) * sizeof(*tk->post_ops));
	if (post_ops == NULL)
		goto fail;

	tk->post_ops = post_ops;
	post_ops[tk->post_ops_len] = op;
	tk->post_ops_len++;
	gen_tk->flags &= ~EC_TK_F_BUILT;

	return 0;

fail:
	ec_tk_free(op);
	return ret;
}

/* add parenthesis symbols */
int ec_tk_expr_add_parenthesis(struct ec_tk *gen_tk,
	struct ec_tk *open, struct ec_tk *close)
{
	struct ec_tk_expr *tk = (struct ec_tk_expr *)gen_tk;
	struct ec_tk **open_ops, **close_ops;
	int ret;

	// XXX check tk type

	ret = -EINVAL;
	if (tk == NULL || open == NULL || close == NULL)
		goto fail;
	ret = -EPERM;
	if (gen_tk->flags & EC_TK_F_BUILT)
		goto fail;;

	ret = -ENOMEM;
	open_ops = ec_realloc(tk->open_ops,
		(tk->paren_len + 1) * sizeof(*tk->open_ops));
	if (open_ops == NULL)
		goto fail;
	close_ops = ec_realloc(tk->close_ops,
		(tk->paren_len + 1) * sizeof(*tk->close_ops));
	if (close_ops == NULL)
		goto fail;

	tk->open_ops = open_ops;
	tk->close_ops = close_ops;
	open_ops[tk->paren_len] = open;
	close_ops[tk->paren_len] = close;
	tk->paren_len++;
	gen_tk->flags &= ~EC_TK_F_BUILT;

	return 0;

fail:
	ec_tk_free(open);
	ec_tk_free(close);
	return ret;
}

enum expr_tk_type {
	NONE,
	VAL,
	BIN_OP,
	PRE_OP,
	POST_OP,
	PAREN_OPEN,
	PAREN_CLOSE,
};
static enum expr_tk_type get_tk_type(const struct ec_tk *expr_gen_tk,
	const struct ec_tk *check_tk)
{
	struct ec_tk_expr *expr_tk = (struct ec_tk_expr *)expr_gen_tk;
	size_t i;

	if (check_tk == expr_tk->val_tk)
		return VAL;

	for (i = 0; i < expr_tk->bin_ops_len; i++) {
		if (check_tk == expr_tk->bin_ops[i])
			return BIN_OP;
	}
	for (i = 0; i < expr_tk->pre_ops_len; i++) {
		if (check_tk == expr_tk->pre_ops[i])
			return PRE_OP;
	}
	for (i = 0; i < expr_tk->post_ops_len; i++) {
		if (check_tk == expr_tk->post_ops[i])
			return POST_OP;
	}

	for (i = 0; i < expr_tk->paren_len; i++) {
		if (check_tk == expr_tk->open_ops[i])
			return PAREN_OPEN;
	}
	for (i = 0; i < expr_tk->paren_len; i++) {
		if (check_tk == expr_tk->close_ops[i])
			return PAREN_CLOSE;
	}

	return NONE;
}

struct result {
	bool has_val;
	void *val;
	const struct ec_parsed_tk *op;
	enum expr_tk_type op_type;
};

/* merge x and y results in x */
static int merge_results(void *userctx,
	const struct ec_tk_expr_eval_ops *ops,
	struct result *x, const struct result *y)
{
	int ret;

	if (y->has_val == 0 && y->op == NULL)
		return 0;
	if (x->has_val == 0 && x->op == NULL) {
		*x = *y;
		return 0;
	}

	if (x->has_val && x->op == NULL && y->has_val && y->op != NULL) {
		ret = ops->eval_bin_op(&x->val, userctx, x->val, y->op, y->val);
		if (ret < 0)
			return ret;

		return 0;
	}

	if (x->has_val == 0 && x->op != NULL && y->has_val && y->op == NULL) {
		if (x->op_type == PRE_OP) {
			ret = ops->eval_pre_op(&x->val, userctx, y->val, x->op);
			if (ret < 0)
				return ret;
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
		ret = ops->eval_post_op(&x->val, userctx, x->val, y->op);
		if (ret < 0)
			return ret;

		return 0;
	}

	assert(true); /* we should not get here */
	return -EINVAL;
}

static int eval_expression(struct result *result,
	void *userctx,
	const struct ec_tk_expr_eval_ops *ops,
	const struct ec_tk *expr_gen_tk,
	const struct ec_parsed_tk *parsed_tk)

{
	struct ec_parsed_tk *open = NULL, *close = NULL;
	struct result child_result;
	struct ec_parsed_tk *child;
	enum expr_tk_type type;
	int ret;

	memset(result, 0, sizeof(*result));
	memset(&child_result, 0, sizeof(child_result));

	type = get_tk_type(expr_gen_tk, parsed_tk->tk);
	if (type == VAL) {
		ret = ops->eval_var(&result->val, userctx, parsed_tk);
		if (ret < 0)
			goto fail;
		result->has_val = 1;
	} else if (type == PRE_OP || type == POST_OP || type == BIN_OP) {
		result->op = parsed_tk;
		result->op_type = type;
	}

	TAILQ_FOREACH(child, &parsed_tk->children, next) {

		type = get_tk_type(expr_gen_tk, child->tk);
		if (type == PAREN_OPEN) {
			open = child;
			continue;
		} else if (type == PAREN_CLOSE) {
			close = child;
			continue;
		}

		ret = eval_expression(&child_result, userctx, ops,
			expr_gen_tk, child);
		if (ret < 0)
			goto fail;

		ret = merge_results(userctx, ops, result, &child_result);
		if (ret < 0)
			goto fail;

		memset(&child_result, 0, sizeof(child_result));
	}

	if (open != NULL && close != NULL) {
		ret = ops->eval_parenthesis(&result->val, userctx, open, close,
			result->val);
		if (ret < 0)
			goto fail;
	}

	return 0;

fail:
	if (result->has_val)
		ops->eval_free(result->val, userctx);
	if (child_result.has_val)
		ops->eval_free(child_result.val, userctx);
	memset(result, 0, sizeof(*result));

	return ret;
}

int ec_tk_expr_eval(void **user_result, const struct ec_tk *tk,
	struct ec_parsed_tk *parsed, const struct ec_tk_expr_eval_ops *ops,
	void *userctx)
{
	struct result result;
	int ret;

	if (ops == NULL || ops->eval_var == NULL || ops->eval_pre_op == NULL ||
			ops->eval_post_op == NULL || ops->eval_bin_op == NULL ||
			ops->eval_parenthesis == NULL || ops->eval_free == NULL)
		return -EINVAL;

	if (!ec_parsed_tk_matches(parsed))
		return -EINVAL;

	ec_parsed_tk_dump(stdout, parsed); //XXX
	ret = eval_expression(&result, userctx, ops, tk, parsed);
	if (ret < 0)
		return ret;

	assert(result.has_val);
	assert(result.op == NULL);
	*user_result = result.val;

	return 0;
}

/* the test case is in a separate file ecoli_tk_expr_test.c */
