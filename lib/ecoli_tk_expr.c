/*
 * Copyright (c) 2016, Olivier MATZ <zer0@droids-corp.org>
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

struct ec_tk_expr {
	struct ec_tk gen;
	struct ec_tk *child;
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

	ec_tk_free(tk->child);
}

static struct ec_tk_ops ec_tk_expr_ops = {
	.typename = "expr",
	.parse = ec_tk_expr_parse,
	.complete = ec_tk_expr_complete,
	.free_priv = ec_tk_expr_free_priv,
};

struct ec_tk *ec_tk_expr_new(const char *id)
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

	if (val_tk == NULL)
		return -EINVAL;
	if (tk->val_tk != NULL)
		return -EEXIST;

	tk->val_tk = val_tk;

	return 0;
}

int ec_tk_expr_add_bin_op(struct ec_tk *gen_tk, struct ec_tk *op)
{
	struct ec_tk_expr *tk = (struct ec_tk_expr *)gen_tk;
	struct ec_tk **bin_ops;

	// XXX check tk type

	if (tk == NULL || op == NULL)
		return -EINVAL;
	if (gen_tk->flags & EC_TK_F_INITIALIZED)
		return -EPERM;

	bin_ops = ec_realloc(tk->bin_ops,
		(tk->bin_ops_len + 1) * sizeof(*tk->bin_ops));
	if (bin_ops == NULL)
		return -1;

	tk->bin_ops = bin_ops;
	bin_ops[tk->bin_ops_len] = op;
	tk->bin_ops_len++;

	return 0;
}

int ec_tk_expr_add_pre_op(struct ec_tk *gen_tk, struct ec_tk *op)
{
	struct ec_tk_expr *tk = (struct ec_tk_expr *)gen_tk;
	struct ec_tk **pre_ops;

	// XXX check tk type

	if (tk == NULL || op == NULL)
		return -EINVAL;
	if (gen_tk->flags & EC_TK_F_INITIALIZED)
		return -EPERM;

	pre_ops = ec_realloc(tk->pre_ops,
		(tk->pre_ops_len + 1) * sizeof(*tk->pre_ops));
	if (pre_ops == NULL)
		return -1;

	tk->pre_ops = pre_ops;
	pre_ops[tk->pre_ops_len] = op;
	tk->pre_ops_len++;

	return 0;
}

int ec_tk_expr_add_post_op(struct ec_tk *gen_tk, struct ec_tk *op)
{
	struct ec_tk_expr *tk = (struct ec_tk_expr *)gen_tk;
	struct ec_tk **post_ops;

	// XXX check tk type

	if (tk == NULL || op == NULL)
		return -EINVAL;
	if (gen_tk->flags & EC_TK_F_INITIALIZED)
		return -EPERM;

	post_ops = ec_realloc(tk->post_ops,
		(tk->post_ops_len + 1) * sizeof(*tk->post_ops));
	if (post_ops == NULL)
		return -1;

	tk->post_ops = post_ops;
	post_ops[tk->post_ops_len] = op;
	tk->post_ops_len++;

	return 0;
}

int ec_tk_expr_add_parenthesis(struct ec_tk *gen_tk,
	struct ec_tk *open, struct ec_tk *close)
{
	struct ec_tk_expr *tk = (struct ec_tk_expr *)gen_tk;
	struct ec_tk **open_ops, **close_ops;

	// XXX check tk type

	if (tk == NULL || open == NULL || close == NULL)
		return -EINVAL;
	if (gen_tk->flags & EC_TK_F_INITIALIZED)
		return -EPERM;

	open_ops = ec_realloc(tk->open_ops,
		(tk->paren_len + 1) * sizeof(*tk->open_ops));
	if (open_ops == NULL)
		return -1;
	close_ops = ec_realloc(tk->close_ops,
		(tk->paren_len + 1) * sizeof(*tk->close_ops));
	if (close_ops == NULL)
		return -1;

	tk->open_ops = open_ops;
	tk->close_ops = close_ops;
	open_ops[tk->paren_len] = open;
	close_ops[tk->paren_len] = close;
	tk->paren_len++;

	return 0;
}

int ec_tk_expr_start(struct ec_tk *gen_tk)
{
	struct ec_tk_expr *tk = (struct ec_tk_expr *)gen_tk;
	struct ec_tk *term = NULL, *prev = NULL, *expr = NULL,
		*val = NULL, *pre_op = NULL, *post_op = NULL,
		*post = NULL, *final = NULL, *next = NULL;
	unsigned int i;

	if (tk->val_tk == NULL)
		return -EINVAL;
	if (gen_tk->flags & EC_TK_F_INITIALIZED)
		return -EPERM;
	if (tk->bin_ops_len == 0 && tk->pre_ops_len == 0 &&
			tk->post_ops_len == 0)
		return -EINVAL;

	/* create the object, we will initialize it later: this is
	 * needed because we have a circular dependency */
	expr = ec_tk_seq_new("expr");

	val = ec_tk_int("val", 0, UCHAR_MAX, 0);

	/* prefix unary operators */
	pre_op = ec_tk_or_new("pre-op");
	for (i = 0; i < tk->pre_ops_len; i++) {
		if (ec_tk_or_add(pre_op, ec_tk_clone(tk->pre_ops[i])) < 0)
			goto fail;
	}
	if (ec_tk_or_start(pre_op) < 0)
		goto fail;

	/* suffix unary operators */
	post_op = ec_tk_or_new("post-op");
	for (i = 0; i < tk->post_ops_len; i++) {
		if (ec_tk_or_add(post_op, ec_tk_clone(tk->post_ops[i])) < 0)
			goto fail;
	}
	if (ec_tk_or_start(post_op) < 0)
		goto fail;

	term = ec_tk_or_new("term");
	if (ec_tk_or_add(term, ec_tk_clone(val)) < 0)
		goto fail;
	if (ec_tk_or_add(term,
		ec_tk_seq(NULL,
			ec_tk_clone(pre_op),
			ec_tk_clone(expr),
			EC_TK_ENDLIST)) < 0)
		goto fail;
	for (i = 0; i < tk->paren_len; i++) {
		if (ec_tk_or_add(term, ec_tk_seq(NULL,
					ec_tk_clone(tk->open_ops[i]),
					ec_tk_clone(expr),
					ec_tk_clone(tk->close_ops[i]),
					EC_TK_ENDLIST)) < 0)
			goto fail;
	}

	prev = term;
	term = NULL;
	for (i = 0; i < tk->bin_ops_len; i++) {
		next = ec_tk_seq("next",
			ec_tk_clone(prev),
			ec_tk_many_new(NULL,
				ec_tk_seq(NULL,
					ec_tk_clone(tk->bin_ops[i]),
					ec_tk_clone(prev),
					EC_TK_ENDLIST
				),
				0, 0
			),
			EC_TK_ENDLIST
		);
		prev = next;
	}

	final = ec_tk_seq("final",
		ec_tk_clone(next),
		ec_tk_many_new(NULL, ec_tk_clone(post_op), 0, 0),
		EC_TK_ENDLIST
	);

	tk->child = final;

	gen_tk->flags |= EC_TK_F_INITIALIZED;

	return 0;

fail:
	ec_tk_free(val);
	ec_tk_free(pre_op);
	ec_tk_free(post_op);
	ec_tk_free(term);
	ec_tk_free(post);
	return -1;
}

static int ec_tk_expr_testcase(void)
{
	struct ec_tk *term, *factor, *expr, *tk, *val,
		*pre_op, *post_op, *post, *final;
	int ret = 0;

	// XXX check all APIs: pointers are "moved", they are freed by
	// the callee

	/* Example that generates an expression "manually". We keep it
	 * here for reference. */

	/* create the object, we will initialize it later: this is
	 * needed because we have a circular dependency */
	expr = ec_tk_seq_new("expr");

	val = ec_tk_int("val", 0, UCHAR_MAX, 0);

	/* reverse bits */
	pre_op = ec_tk_or("pre-op",
		ec_tk_str(NULL, "~"),
		EC_TK_ENDLIST
	);

	/* factorial */
	post_op = ec_tk_or("post-op",
		ec_tk_str(NULL, "!"),
		EC_TK_ENDLIST
	);

	term = ec_tk_or("term",
		ec_tk_clone(val),
		ec_tk_seq(NULL,
			ec_tk_str(NULL, "("),
			ec_tk_clone(expr),
			ec_tk_str(NULL, ")"),
			EC_TK_ENDLIST
		),
		ec_tk_seq(NULL,
			ec_tk_clone(pre_op),
			ec_tk_clone(expr),
			EC_TK_ENDLIST
		),
		EC_TK_ENDLIST
	);

	factor = ec_tk_seq("factor",
		ec_tk_clone(term),
		ec_tk_many_new(NULL,
			ec_tk_seq(NULL,
				ec_tk_str(NULL, "+"),
				ec_tk_clone(term),
				EC_TK_ENDLIST
			),
			0, 0
		),
		EC_TK_ENDLIST
	);

	post = ec_tk_seq("post",
		ec_tk_clone(factor),
		ec_tk_many_new(NULL,
			ec_tk_seq(NULL,
				ec_tk_str(NULL, "*"),
				ec_tk_clone(factor),
				EC_TK_ENDLIST
			),
			0, 0
		),
		EC_TK_ENDLIST
	);

	final = ec_tk_seq("final",
		ec_tk_clone(post),
		ec_tk_many_new(NULL, ec_tk_clone(post_op), 0, 0),
		EC_TK_ENDLIST
	);

	/* free the initial references */
	ec_tk_free(val);
	ec_tk_free(pre_op);
	ec_tk_free(post_op);
	ec_tk_free(term);
	ec_tk_free(factor);
	ec_tk_free(post);

	if (ec_tk_seq_add(expr, ec_tk_clone(final)) < 0) {
		ec_tk_free(final);
		ec_tk_free(expr);
		return -1;
	}

	ec_tk_free(final);

	ret |= EC_TEST_CHECK_TK_PARSE(expr, 1, "1", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(expr, 1, "1", "*", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(expr, 3, "1", "*", "1", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(expr, 3, "1", "+", "1", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(expr, 5, "1", "*", "1", "+", "1",
		EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(
		expr, 10, "~", "(", "1", "*", "(", "1", "+", "1", ")", ")",
		EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(expr, 4, "1", "+", "~", "1",
		EC_TK_ENDLIST);

	ec_tk_free(expr);

	tk = ec_tk_expr_new(NULL);
	ec_tk_expr_set_val_tk(tk, ec_tk_int(NULL, 0, UCHAR_MAX, 0));
	ec_tk_expr_add_bin_op(tk, ec_tk_str(NULL, "+"));
	ec_tk_expr_add_bin_op(tk, ec_tk_str(NULL, "*"));
	ec_tk_expr_add_pre_op(tk, ec_tk_str(NULL, "~"));
	ec_tk_expr_add_pre_op(tk, ec_tk_str(NULL, "!"));
	ec_tk_expr_add_parenthesis(tk, ec_tk_str(NULL, "("),
		ec_tk_str(NULL, ")"));
	ec_tk_expr_start(tk);

	ret |= EC_TEST_CHECK_TK_PARSE(expr, 1, "1", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(expr, 1, "1", "*", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(expr, 3, "1", "*", "1", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(expr, 3, "1", "+", "1", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(expr, 5, "1", "*", "1", "+", "1",
		EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(
		expr, 10, "~", "(", "1", "*", "(", "1", "+", "1", ")", ")",
		EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(expr, 4, "1", "+", "~", "1",
		EC_TK_ENDLIST);
	ec_tk_free(tk);

	return ret;
}

static struct ec_test ec_tk_expr_test = {
	.name = "tk_expr",
	.test = ec_tk_expr_testcase,
};

EC_REGISTER_TEST(ec_tk_expr_test);
