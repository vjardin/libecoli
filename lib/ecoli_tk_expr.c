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
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_tk.h>
#include <ecoli_tk_str.h>
#include <ecoli_tk_seq.h>
#include <ecoli_tk_many.h>
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

	/* XXX use assert or check like this ? */
	if (tk == NULL || op == NULL)
		return -EINVAL;

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

	/* XXX use assert or check like this ? */
	if (tk == NULL || op == NULL)
		return -EINVAL;

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

	/* XXX use assert or check like this ? */
	if (tk == NULL || op == NULL)
		return -EINVAL;

	post_ops = ec_realloc(tk->post_ops,
		(tk->post_ops_len + 1) * sizeof(*tk->post_ops));
	if (post_ops == NULL)
		return -1;

	tk->post_ops = post_ops;
	post_ops[tk->post_ops_len] = op;
	tk->post_ops_len++;

	return 0;
}

int ec_tk_expr_start(struct ec_tk *gen_tk)
{
	struct ec_tk_expr *tk = (struct ec_tk_expr *)gen_tk;

	if (tk->val_tk == NULL)
		return -EINVAL;
	if (tk->bin_ops_len == 0 || tk->pre_ops_len == 0 ||
			tk->post_ops_len == 0)
		return -EINVAL;

	return 0;
}

struct ec_tk *ec_tk_expr(const char *id, struct ec_tk *val_tk,
	const char *bin_ops)
{
	struct ec_tk_expr *tk = NULL;
	struct ec_tk *gen_tk = NULL, *child = NULL, *sub_expr = NULL;
	char *op = NULL;

	gen_tk = ec_tk_expr_new(id);
	if (gen_tk == NULL)
		goto fail;
	tk = (struct ec_tk_expr *)gen_tk;

	if (bin_ops == NULL || bin_ops[0] == '\0') {
		child = val_tk;
	} else {
		/* recursively create an expr tk without the first operator */
		sub_expr = ec_tk_expr(NULL,
			ec_tk_clone(val_tk),
			&bin_ops[1]);
		if (sub_expr == NULL)
			goto fail;

		op = ec_strdup(bin_ops);
		if (op == NULL)
			goto fail;
		op[1] = '\0';

		/* we match:
		 *   <subexpr> (<op> <subexpr>)*
		 */
		child = ec_tk_seq_new_list(NULL,
			ec_tk_clone(sub_expr),
			ec_tk_many_new(NULL,
				ec_tk_seq_new_list(NULL,
					ec_tk_str_new(NULL, op),
					ec_tk_clone(sub_expr),
					EC_TK_ENDLIST
				),
				0, 0
			),
			EC_TK_ENDLIST
		);
		ec_free(op);
		op = NULL;

		/* remove initial reference */
		ec_tk_free(sub_expr);

		if (child == NULL)
			goto fail;

		ec_tk_free(val_tk);
	}

	tk->child = child;

	return &tk->gen;

fail:
	ec_free(op);
	ec_tk_free(child);
	ec_tk_free(val_tk);
	ec_tk_free(gen_tk);
	return NULL;
}

static int ec_tk_expr_testcase(void)
{
	struct ec_tk *tk, *tk2, *val_tk;
	int ret = 0;

	val_tk = ec_tk_str_new(NULL, "val");
	tk = ec_tk_seq_new_list(NULL,
		ec_tk_clone(val_tk),
		ec_tk_many_new(NULL,
			ec_tk_seq_new_list(NULL,
				ec_tk_str_new(NULL, "*"),
				ec_tk_clone(val_tk),
				EC_TK_ENDLIST
			),
			0, 0
		),
		EC_TK_ENDLIST
	);
	ec_tk_free(val_tk);
	val_tk = NULL;

	tk2 = ec_tk_seq_new_list(NULL,
		ec_tk_clone(tk),
		ec_tk_many_new(NULL,
			ec_tk_seq_new_list(NULL,
				ec_tk_str_new(NULL, "+"),
				ec_tk_clone(tk),
				EC_TK_ENDLIST
			),
			0, 0
		),
		EC_TK_ENDLIST
	);


//		"/*-+");
	if (tk2 == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}

	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "val", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "val", "*", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 3, "val", "*", "val", EC_TK_ENDLIST);

	ret |= EC_TEST_CHECK_TK_PARSE(tk2, 1, "val", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk2, 1, "val", "*", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk2, 3, "val", "*", "val", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk2, 3, "val", "+", "val", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk2, 5, "val", "*", "val", "+", "val",
		EC_TK_ENDLIST);

	ec_tk_free(tk);
	ec_tk_free(tk2);

	tk = ec_tk_expr(NULL, ec_tk_str_new(NULL, "val"), "*+");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 5, "val", "*", "val", "+", "val",
		EC_TK_ENDLIST);
	ec_tk_free(tk);

	return ret;
}

static struct ec_test ec_tk_expr_test = {
	.name = "tk_expr",
	.test = ec_tk_expr_testcase,
};

EC_REGISTER_TEST(ec_tk_expr_test);
