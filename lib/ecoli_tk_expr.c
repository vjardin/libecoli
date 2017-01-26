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
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_tk.h>
#include <ecoli_tk_str.h>
#include <ecoli_tk_int.h>
#include <ecoli_tk_seq.h>
#include <ecoli_tk_many.h>
#include <ecoli_tk_or.h>
#include <ecoli_tk_expr.h>
#include <ecoli_tk_weakref.h>

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

	term = ec_tk_or("term");
	if (term == NULL)
		goto fail;
	if (ec_tk_or_add(term, ec_tk_clone(tk->val_tk)) < 0)
		goto fail;
	if (ec_tk_or_add(term,
		EC_TK_SEQ(NULL,
			ec_tk_clone(pre_op),
			ec_tk_clone(weak))) < 0)
		goto fail;
	for (i = 0; i < tk->paren_len; i++) {
		if (ec_tk_or_add(term, EC_TK_SEQ(NULL,
					ec_tk_clone(tk->open_ops[i]),
					ec_tk_clone(weak),
					ec_tk_clone(tk->close_ops[i]))) < 0)
			goto fail;
	}

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

	expr = EC_TK_SEQ("expr",
		ec_tk_clone(term),
		ec_tk_many(NULL, ec_tk_clone(post_op), 0, 0)
	);
	if (expr == NULL)
		goto fail;

	/* free the initial references */
	ec_tk_free(pre_op);
	pre_op = NULL;
	ec_tk_free(post_op);
	post_op = NULL;
	ec_tk_free(term);
	term = NULL;
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

static struct ec_tk_ops ec_tk_expr_ops = {
	.typename = "expr",
	.build = ec_tk_expr_build,
	.parse = ec_tk_expr_parse,
	.complete = ec_tk_expr_complete,
	.free_priv = ec_tk_expr_free_priv,
};

struct ec_tk *ec_tk_expr(const char *id)
{
	struct ec_tk_expr *tk = NULL;
	struct ec_tk *gen_tk = NULL;

	gen_tk = ec_tk_new(id, &ec_tk_expr_ops, sizeof(*tk));
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

static int ec_tk_expr_testcase_manual(void)
{
	struct ec_tk *term = NULL, *factor = NULL, *expr = NULL, *val = NULL,
		*pre_op = NULL, *post_op = NULL, *post = NULL, *weak = NULL;
	int ret = 0;

	// XXX check all APIs: pointers are "moved", they are freed by
	// the callee

	/* Example that generates an expression "manually". We keep it
	 * here for reference. */

	weak = ec_tk_weakref_empty("weak");
	if (weak == NULL)
		return -1;

	/* reverse bits */
	pre_op = EC_TK_OR("pre-op",
		ec_tk_str(NULL, "~")
	);

	/* factorial */
	post_op = EC_TK_OR("post-op",
		ec_tk_str(NULL, "!")
	);

	val = ec_tk_int("val", 0, UCHAR_MAX, 0);
	term = EC_TK_OR("term",
		val,
		EC_TK_SEQ(NULL,
			ec_tk_str(NULL, "("),
			ec_tk_clone(weak),
			ec_tk_str(NULL, ")")
		),
		EC_TK_SEQ(NULL,
			ec_tk_clone(pre_op),
			ec_tk_clone(weak)
		)
	);
	val = NULL;

	factor = EC_TK_SEQ("factor",
		ec_tk_clone(term),
		ec_tk_many(NULL,
			EC_TK_SEQ(NULL,
				ec_tk_str(NULL, "+"),
				ec_tk_clone(term)
			),
			0, 0
		)
	);

	post = EC_TK_SEQ("post",
		ec_tk_clone(factor),
		ec_tk_many(NULL,
			EC_TK_SEQ(NULL,
				ec_tk_str(NULL, "*"),
				ec_tk_clone(factor)
			),
			0, 0
		)
	);

	expr = EC_TK_SEQ("expr",
		ec_tk_clone(post),
		ec_tk_many(NULL, ec_tk_clone(post_op), 0, 0)
	);

	/* free the initial references */
	ec_tk_free(pre_op);
	pre_op = NULL;
	ec_tk_free(post_op);
	post_op = NULL;
	ec_tk_free(term);
	term = NULL;
	ec_tk_free(factor);
	factor = NULL;
	ec_tk_free(post);
	post = NULL;

	/* no need to clone here, the node is not consumed */
	if (ec_tk_weakref_set(weak, expr) < 0)
		goto fail;
	ec_tk_free(weak);
	weak = NULL;

	ret |= EC_TEST_CHECK_TK_PARSE(expr, 1, "1");
	ret |= EC_TEST_CHECK_TK_PARSE(expr, 1, "1", "*");
	ret |= EC_TEST_CHECK_TK_PARSE(expr, 3, "1", "*", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(expr, 3, "1", "+", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(expr, 5, "1", "*", "1", "+", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(
		expr, 10, "~", "(", "1", "*", "(", "1", "+", "1", ")", ")");
	ret |= EC_TEST_CHECK_TK_PARSE(expr, 4, "1", "+", "~", "1");

	ec_tk_free(expr);

	return ret;

fail:
	ec_tk_free(term);
	ec_tk_free(factor);
	ec_tk_free(expr);
	ec_tk_free(val);
	ec_tk_free(pre_op);
	ec_tk_free(post_op);
	ec_tk_free(post);
	ec_tk_free(weak);
	return 0;
}

static int ec_tk_expr_testcase(void)
{
	struct ec_tk *tk;
	int ret;

	ret = ec_tk_expr_testcase_manual();
	if (ret < 0)
		return ret;

	tk = ec_tk_expr(NULL);
	if (tk == NULL)
		return -1;

	ec_tk_expr_set_val_tk(tk, ec_tk_int(NULL, 0, UCHAR_MAX, 0));
	ec_tk_expr_add_bin_op(tk, ec_tk_str(NULL, "+"));
	ec_tk_expr_add_bin_op(tk, ec_tk_str(NULL, "*"));
	ec_tk_expr_add_pre_op(tk, ec_tk_str(NULL, "~"));
	ec_tk_expr_add_pre_op(tk, ec_tk_str(NULL, "!"));
	ec_tk_expr_add_parenthesis(tk, ec_tk_str(NULL, "("),
		ec_tk_str(NULL, ")"));
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "1", "*");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 3, "1", "*", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 3, "1", "+", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 5, "1", "*", "1", "+", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(
		tk, 10, "~", "(", "1", "*", "(", "1", "+", "1", ")", ")");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 4, "1", "+", "~", "1");
	ec_tk_free(tk);

	return ret;
}

static struct ec_test ec_tk_expr_test = {
	.name = "tk_expr",
	.test = ec_tk_expr_testcase,
};

EC_REGISTER_TEST(ec_tk_expr_test);
