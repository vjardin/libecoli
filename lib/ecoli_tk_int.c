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
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_tk.h>
#include <ecoli_tk_int.h>
#include <ecoli_test.h>

static size_t parse_llint(struct ec_tk_int *tk, const char *str,
	long long *val)
{
	char *endptr;

	errno = 0;
	*val = strtoll(str, &endptr, tk->base);

	/* starts with a space */
	if (isspace(str[0]))
		return 0;

	/* out of range */
	if ((errno == ERANGE && (*val == LLONG_MAX || *val == LLONG_MIN)) ||
			(errno != 0 && *val == 0))
		return 0;

	if (*val < tk->min || *val > tk->max)
		return 0;

	return endptr - str;
}

static struct ec_parsed_tk *parse(const struct ec_tk *gen_tk,
	const char *str)
{
	struct ec_tk_int *tk = (struct ec_tk_int *)gen_tk;
	struct ec_parsed_tk *parsed_tk;
	long long val;
	size_t len;

	len = parse_llint(tk, str, &val);
	if (len == 0)
		return NULL;

	parsed_tk = ec_parsed_tk_new(gen_tk);
	if (parsed_tk == NULL)
		return NULL;

	parsed_tk->str = ec_strndup(str, len);

	return parsed_tk;
}

static struct ec_tk_ops int_ops = {
	.parse = parse,
};

struct ec_tk *ec_tk_int_new(const char *id, long long int min,
	long long int max, unsigned int base)
{
	struct ec_tk_int *tk = NULL;

	tk = (struct ec_tk_int *)ec_tk_new(id, &int_ops, sizeof(*tk));
	if (tk == NULL)
		return NULL;

	tk->min = min;
	tk->max = max;
	tk->base = base;

	return &tk->gen;
}

long long ec_tk_int_getval(struct ec_tk *gen_tk, const char *str)
{
	struct ec_tk_int *tk = (struct ec_tk_int *)gen_tk;
	long long val = 0;

	// XXX check type here
	// if gen_tk->type != int fail

	parse_llint(tk, str, &val);

	return val;
}

static int testcase(void)
{
	struct ec_parsed_tk *p;
	struct ec_tk *tk;
	const char *s;
	int ret = 0;

	tk = ec_tk_int_new(NULL, 0, 256, 0);
	if (tk == NULL) {
		printf("cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "0", "0");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "256", "256");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "0x100", "0x100");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "-1", NULL);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "0x101", NULL);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, " 1", NULL);

	p = ec_tk_parse(tk, "0");
	s = ec_parsed_tk_to_string(p);
	if (s == NULL) {
		TEST_ERR();
	} else {
		if (ec_tk_int_getval(tk, s) != 0)
			TEST_ERR();
	}

	p = ec_tk_parse(tk, "10");
	s = ec_parsed_tk_to_string(p);
	if (s == NULL) {
		TEST_ERR();
	} else {
		if (ec_tk_int_getval(tk, s) != 10)
			TEST_ERR();
	}

	ec_tk_free(tk);

	tk = ec_tk_int_new(NULL, -1, LLONG_MAX, 16);
	if (tk == NULL) {
		printf("cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "0", "0");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "-1", "-1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "7fffffffffffffff",
		"7fffffffffffffff");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "0x7fffffffffffffff",
		"0x7fffffffffffffff");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "-2", NULL);

	p = ec_tk_parse(tk, "10");
	s = ec_parsed_tk_to_string(p);
	if (s == NULL) {
		TEST_ERR();
	} else {
		if (ec_tk_int_getval(tk, s) != 16)
			TEST_ERR();
	}

	ec_tk_free(tk);

	tk = ec_tk_int_new(NULL, LLONG_MIN, 0, 10);
	if (tk == NULL) {
		printf("cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "0", "0");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "-1", "-1");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "-9223372036854775808",
		"-9223372036854775808");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "0x0", "0");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "1", NULL);
	ec_tk_free(tk);

	/* /\* test completion *\/ */
	/* tk = ec_tk_int_new(NULL, "foo"); */
	/* if (tk == NULL) { */
	/* 	printf("cannot create tk\n"); */
	/* 	return -1; */
	/* } */
	/* ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "", "foo"); */
	/* ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "f", "oo"); */
	/* ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "foo", ""); */
	/* ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "x", NULL); */
	/* ec_tk_free(tk); */

	return ret;
}

static struct ec_test test = {
	.name = "tk_int",
	.test = testcase,
};

EC_REGISTER_TEST(test);
