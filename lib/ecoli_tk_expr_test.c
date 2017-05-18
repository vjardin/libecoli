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

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <assert.h>

#include <ecoli_malloc.h>
#include <ecoli_strvec.h>
#include <ecoli_test.h>
#include <ecoli_tk_int.h>
#include <ecoli_tk_str.h>
#include <ecoli_tk_re_lex.h>
#include <ecoli_tk_expr.h>

struct my_eval_result {
	int val;
};

static int
ec_tk_expr_test_eval_var(void **result, void *userctx,
	const struct ec_parsed_tk *var)
{
	const struct ec_strvec *vec;
	struct my_eval_result *eval;

	(void)userctx;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parsed_tk_strvec(var);
	if (ec_strvec_len(vec) != 1)
		return -EINVAL;

	eval = ec_malloc(sizeof(*eval));
	if (eval == NULL)
		return -ENOMEM;

	eval->val = atoi(ec_strvec_val(vec, 0)); // XXX use strtol
	printf("eval var %d\n", eval->val);
	*result = eval;

	return 0;
}

static int
ec_tk_expr_test_eval_pre_op(void **result, void *userctx, void *operand,
	const struct ec_parsed_tk *operator)
{
	const struct ec_strvec *vec;
	struct my_eval_result *eval = operand;;

	(void)userctx;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parsed_tk_strvec(operator);
	if (ec_strvec_len(vec) != 1)
		return -EINVAL;

	if (!strcmp(ec_strvec_val(vec, 0), "!"))
		eval->val = !eval->val;
	else
		return -EINVAL;

	printf("eval pre_op %d\n", eval->val);
	*result = eval;

	return 0;
}

static int
ec_tk_expr_test_eval_post_op(void **result, void *userctx, void *operand,
	const struct ec_parsed_tk *operator)
{
	const struct ec_strvec *vec;
	struct my_eval_result *eval = operand;;

	(void)userctx;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parsed_tk_strvec(operator);
	if (ec_strvec_len(vec) != 1)
		return -EINVAL;

	if (!strcmp(ec_strvec_val(vec, 0), "^"))
		eval->val = eval->val * eval->val;
	else
		return -EINVAL;

	printf("eval post_op %d\n", eval->val);
	*result = eval;

	return 0;
}

static int
ec_tk_expr_test_eval_bin_op(void **result, void *userctx, void *operand1,
	const struct ec_parsed_tk *operator, void *operand2)

{
	const struct ec_strvec *vec;
	struct my_eval_result *eval1 = operand1;;
	struct my_eval_result *eval2 = operand2;;

	(void)userctx;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parsed_tk_strvec(operator);
	if (ec_strvec_len(vec) != 1)
		return -EINVAL;

	if (!strcmp(ec_strvec_val(vec, 0), "+"))
		eval1->val = eval1->val + eval2->val;
	else if (!strcmp(ec_strvec_val(vec, 0), "*"))
		eval1->val = eval1->val * eval2->val;
	else
		return -EINVAL;

	printf("eval bin_op %d\n", eval1->val);
	ec_free(eval2);
	*result = eval1;

	return 0;
}

static int
ec_tk_expr_test_eval_parenthesis(void **result, void *userctx,
	const struct ec_parsed_tk *open_paren,
	const struct ec_parsed_tk *close_paren,
	void *value)
{
	(void)userctx;
	(void)open_paren;
	(void)close_paren;

	printf("eval paren\n");
	*result = value;

	return 0;
}

static void
ec_tk_expr_test_eval_free(void *result, void *userctx)
{
	(void)userctx;
	ec_free(result);
}

static const struct ec_tk_expr_eval_ops test_ops = {
	.eval_var = ec_tk_expr_test_eval_var,
	.eval_pre_op = ec_tk_expr_test_eval_pre_op,
	.eval_post_op = ec_tk_expr_test_eval_post_op,
	.eval_bin_op = ec_tk_expr_test_eval_bin_op,
	.eval_parenthesis = ec_tk_expr_test_eval_parenthesis,
	.eval_free = ec_tk_expr_test_eval_free,
};

static int ec_tk_expr_test_eval(struct ec_tk *lex_tk,
	const struct ec_tk *expr_tk,
	const char *str, int val)
{
	struct ec_parsed_tk *p;
	void *result;
	struct my_eval_result *eval;
	int ret;

	/* XXX check tk type (again and again) */

	p = ec_tk_parse(lex_tk, str);
	if (p == NULL)
		return -1;

	ret = ec_tk_expr_eval(&result, expr_tk, p, &test_ops, NULL);
	ec_parsed_tk_free(p);
	if (ret < 0)
		return -1;

	/* the parsed value is an integer */
	eval = result;
	assert(eval != NULL);

	printf("result: %d (expected %d)\n", eval->val, val);
	if (eval->val == val)
		ret = 0;
	else
		ret = -1;

	ec_free(eval);

	return ret;
}

static int ec_tk_expr_testcase(void)
{
	struct ec_tk *tk = NULL, *lex_tk = NULL;
	int ret = 0;

	tk = ec_tk_expr("expr");
	if (tk == NULL)
		return -1;

	ec_tk_expr_set_val_tk(tk, ec_tk_int(NULL, 0, UCHAR_MAX, 0));
	ec_tk_expr_add_bin_op(tk, ec_tk_str(NULL, "+"));
	ec_tk_expr_add_bin_op(tk, ec_tk_str(NULL, "*"));
	ec_tk_expr_add_pre_op(tk, ec_tk_str(NULL, "!"));  /* not */
	ec_tk_expr_add_post_op(tk, ec_tk_str(NULL, "^")); /* square */
	ec_tk_expr_add_parenthesis(tk, ec_tk_str(NULL, "("),
		ec_tk_str(NULL, ")"));
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "1", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "1", "*");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 3, "1", "*", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 3, "1", "*", "1", "*");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 4, "1", "+", "!", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 4, "1", "^", "+", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 5, "1", "*", "1", "*", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 5, "1", "*", "1", "+", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 7, "1", "*", "1", "*", "1", "*", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(
		tk, 10, "!", "(", "1", "*", "(", "1", "+", "1", ")", ")");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 5, "1", "+", "!", "1", "^");

	/* prepend a lexer to the expression token */
	lex_tk = ec_tk_re_lex(NULL, ec_tk_clone(tk));
	if (lex_tk == NULL)
		goto fail;

	ret |= ec_tk_re_lex_add(lex_tk, "[0-9]+", 1); /* vars */
	ret |= ec_tk_re_lex_add(lex_tk, "[+*!^()]", 1); /* operators */
	ret |= ec_tk_re_lex_add(lex_tk, "[ 	]+", 0); /* spaces */

	/* valid expressions */
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, 1, "!1");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, 1, "1^");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, 1, "1^ + 1");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, 1, "1 + 4 * (2 + 3^)^");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, 1, "(1)");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, 1, "3*!3+!3*(2+ 2)");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, 1, "!!(!1)^ + !(4 + (2*3))");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, 1, "(1 + 1)^ * 1^");

	/* invalid expressions */
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, -1, "");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, -1, "()");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, -1, "(");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, -1, ")");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, -1, "+1");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, -1, "1+");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, -1, "1+*1");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, -1, "1+(1*1");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, -1, "1+!1!1)");

	ret |= ec_tk_expr_test_eval(lex_tk, tk, "1^", 1);
	ret |= ec_tk_expr_test_eval(lex_tk, tk, "2^", 4);
	ret |= ec_tk_expr_test_eval(lex_tk, tk, "!1", 0);
	ret |= ec_tk_expr_test_eval(lex_tk, tk, "!0", 1);

	ret |= ec_tk_expr_test_eval(lex_tk, tk, "1+1", 2);
	ret |= ec_tk_expr_test_eval(lex_tk, tk, "1+1*2", 4);
	ret |= ec_tk_expr_test_eval(lex_tk, tk, "2 * 2^", 8);
	ret |= ec_tk_expr_test_eval(lex_tk, tk, "(1 + !0)^ * !0^", 4);
	ret |= ec_tk_expr_test_eval(lex_tk, tk, "(1 + !1) * 3", 3);

	ec_tk_free(tk);
	ec_tk_free(lex_tk);

	return ret;

fail:
	ec_tk_free(lex_tk);
	ec_tk_free(tk);
	return -1;
}

static struct ec_test ec_tk_expr_test = {
	.name = "tk_expr",
	.test = ec_tk_expr_testcase,
};

EC_TEST_REGISTER(ec_tk_expr_test);
