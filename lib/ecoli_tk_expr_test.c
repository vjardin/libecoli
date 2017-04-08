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

static struct ec_tk_expr_eval_result
ec_tk_expr_test_eval_var(void *userctx, const struct ec_parsed_tk *var)
{
	const struct ec_strvec *vec;
	struct ec_tk_expr_eval_result res = { .code = -EINVAL, .parsed = NULL };
	struct my_eval_result *eval;

	(void)userctx;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parsed_tk_strvec(var);
	if (ec_strvec_len(vec) != 1)
		return res;

	res.code = -ENOMEM;
	eval = ec_malloc(sizeof(*eval));
	if (eval == NULL)
		return res;

	eval->val = atoi(ec_strvec_val(vec, 0)); // XXX use strtol
	printf("eval var %d\n", eval->val);
	res.code = 0;
	res.parsed = eval;

	return res;
}

static struct ec_tk_expr_eval_result
ec_tk_expr_test_eval_pre_op(void *userctx,
	struct ec_tk_expr_eval_result operand,
	const struct ec_parsed_tk *operator)
{
	const struct ec_strvec *vec;
	struct ec_tk_expr_eval_result res = { .code = -EINVAL, .parsed = NULL };
	struct my_eval_result *eval = operand.parsed;;

	(void)userctx;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parsed_tk_strvec(operator);
	if (ec_strvec_len(vec) != 1) {
		ec_free(eval);
		return res;
	}

	if (!strcmp(ec_strvec_val(vec, 0), "~")) {
		eval->val = !eval->val;
	} else {
		ec_free(eval);
		return res;
	}

	printf("eval pre_op %d\n", eval->val);
	res.code = 0;
	res.parsed = eval;

	return res;
}

static struct ec_tk_expr_eval_result
ec_tk_expr_test_eval_post_op(void *userctx,
	struct ec_tk_expr_eval_result operand,
	const struct ec_parsed_tk *operator)
{
	const struct ec_strvec *vec;
	struct ec_tk_expr_eval_result res = { .code = -EINVAL, .parsed = NULL };
	struct my_eval_result *eval = operand.parsed;;

	(void)userctx;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parsed_tk_strvec(operator);
	if (ec_strvec_len(vec) != 1) {
		ec_free(eval);
		return res;
	}

	if (!strcmp(ec_strvec_val(vec, 0), "!")) {
		eval->val = eval->val * eval->val;
	} else {
		ec_free(eval);
		return res;
	}

	printf("eval post_op %d\n", eval->val);
	res.code = 0;
	res.parsed = eval;

	return res;
}

static struct ec_tk_expr_eval_result
ec_tk_expr_test_eval_bin_op(void *userctx,
	struct ec_tk_expr_eval_result operand1,
	const struct ec_parsed_tk *operator,
	struct ec_tk_expr_eval_result operand2)

{
	const struct ec_strvec *vec;
	struct ec_tk_expr_eval_result res = { .code = -EINVAL, .parsed = NULL };
	struct my_eval_result *eval1 = operand1.parsed;;
	struct my_eval_result *eval2 = operand2.parsed;;

	(void)userctx;

	/* get parsed string vector, it should contain only one str */
	vec = ec_parsed_tk_strvec(operator);
	if (ec_strvec_len(vec) != 1) {
		ec_free(eval1);
		ec_free(eval2);
		return res;
	}

	if (!strcmp(ec_strvec_val(vec, 0), "+")) {
		eval1->val = eval1->val + eval2->val;
	} else if (!strcmp(ec_strvec_val(vec, 0), "*")) {
		eval1->val = eval1->val * eval2->val;
	} else {
		ec_free(eval1);
		ec_free(eval2);
		return res;
	}

	printf("eval bin_op %d\n", eval1->val);
	res.code = 0;
	res.parsed = eval1;

	return res;
}

static const struct ec_tk_expr_eval_ops test_ops = {
	.eval_var = ec_tk_expr_test_eval_var,
	.eval_pre_op = ec_tk_expr_test_eval_pre_op,
	.eval_post_op = ec_tk_expr_test_eval_post_op,
	.eval_bin_op = ec_tk_expr_test_eval_bin_op,
};

static int ec_tk_expr_test_eval(struct ec_tk *lex_tk,
	const struct ec_tk *expr_tk,
	const char *str, int val)
{
	struct ec_tk_expr_eval_result res;
	struct ec_parsed_tk *p;
	struct my_eval_result *eval;
	int ret;

	/* XXX check tk type (again and again) */


	p = ec_tk_parse(lex_tk, str);
	if (p == NULL)
		return -1;

	res = ec_tk_expr_eval(expr_tk, p, &test_ops, NULL);
	ec_parsed_tk_free(p);
	if (res.code < 0)
		return res.code;

	/* the parsed value is an integer */
	eval = res.parsed;
	if (eval == NULL) {
		ret = -1;
	} else {
		printf("result: %d (expected %d)\n", eval->val, val);
		if (eval->val == val)
			ret = 0;
		else
			ret = -1;
	}
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
	ec_tk_expr_add_pre_op(tk, ec_tk_str(NULL, "~"));
	ec_tk_expr_add_post_op(tk, ec_tk_str(NULL, "!"));
	ec_tk_expr_add_parenthesis(tk, ec_tk_str(NULL, "("),
		ec_tk_str(NULL, ")"));
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "1", "*");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 3, "1", "*", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 4, "1", "+", "~", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 4, "1", "!", "+", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 5, "1", "*", "1", "*", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 5, "1", "*", "1", "*", "1", "*", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 5, "1", "*", "1", "+", "1");
	ret |= EC_TEST_CHECK_TK_PARSE(
		tk, 10, "~", "(", "1", "*", "(", "1", "+", "1", ")", ")");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 5, "1", "+", "~", "1", "!");

	/* prepend a lexer to the expression token */
	lex_tk = ec_tk_re_lex(NULL, ec_tk_clone(tk));
	if (lex_tk == NULL)
		goto fail;

	ret |= ec_tk_re_lex_add(lex_tk, "^[0-9]+", 1); /* vars */
	ret |= ec_tk_re_lex_add(lex_tk, "^[+*~!()]", 1); /* operators */
	ret |= ec_tk_re_lex_add(lex_tk, "^[ 	]+", 0); /* spaces */
	if (ret != 0)
		goto fail;

	/* valid expressions */
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, 1, "~1");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, 1, "1!");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, 1, "1! + 1");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, 1, "1 + 4 * (2 + 3!)!");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, 1, "(1)");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, 1, "3*~3+~3*(2+ 2)");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, 1, "~~(~1)! + ~(4 + (2*3))");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, 1, "(1 + 1)! * 1!");

	/* invalid expressions */
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, -1, "");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, -1, "()");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, -1, "(");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, -1, ")");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, -1, "+1");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, -1, "1+");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, -1, "1+*1");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, -1, "1+(1*1");
	ret |= EC_TEST_CHECK_TK_PARSE(lex_tk, -1, "1+~1~1)");
	if (ret)
		exit(ret);

	ret |= ec_tk_expr_test_eval(lex_tk, tk, "2!", 4);
	ret |= ec_tk_expr_test_eval(lex_tk, tk, "1+1", 2);
	ret |= ec_tk_expr_test_eval(lex_tk, tk, "1+1*2", 4);

	// XXX marche pas:
	// le "*" s'applique avant le "!" final
	ret |= ec_tk_expr_test_eval(lex_tk, tk, "2 * 2!", 8);
	/* ret |= ec_tk_expr_test_eval(lex_tk, tk, "(1 + ~0)! * ~0!", 4); */
	/* ret |= ec_tk_expr_test_eval(lex_tk, tk, "(1 + ~1) * 3", 3); */
	/* ret |= ec_tk_expr_test_eval(lex_tk, tk, "~1", 0); */
	printf("exit\n");
	exit(ret);

	/*
	  idee:
	  eval_expression() retourne soit:
	  - NONE
	  - *_OP, parsed_tk
	  - VAL, eval(parsed_tk)
	  */


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

EC_REGISTER_TEST(ec_tk_expr_test);
