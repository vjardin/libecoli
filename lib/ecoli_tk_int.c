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

#include <ecoli_log.h>
#include <ecoli_malloc.h>
#include <ecoli_strvec.h>
#include <ecoli_tk.h>
#include <ecoli_tk_int.h>
#include <ecoli_test.h>

struct ec_tk_int {
	struct ec_tk gen;
	long long int min;
	long long int max;
	unsigned int base;
};

static int parse_llint(struct ec_tk_int *tk, const char *str,
	long long *val)
{
	char *endptr;

	errno = 0;
	*val = strtoll(str, &endptr, tk->base);

	/* out of range */
	if ((errno == ERANGE && (*val == LLONG_MAX || *val == LLONG_MIN)) ||
			(errno != 0 && *val == 0))
		return -1;

	if (*val < tk->min || *val > tk->max)
		return -1;

	if (*endptr != 0)
		return -1;

	return 0;
}

static struct ec_parsed_tk *ec_tk_int_parse(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_int *tk = (struct ec_tk_int *)gen_tk;
	struct ec_parsed_tk *parsed_tk;
	struct ec_strvec *match_strvec;
	const char *str;
	long long val;

	parsed_tk = ec_parsed_tk_new();
	if (parsed_tk == NULL)
		goto fail;

	if (ec_strvec_len(strvec) == 0)
		return parsed_tk;

	str = ec_strvec_val(strvec, 0);
	if (parse_llint(tk, str, &val) < 0)
		return parsed_tk;

	match_strvec = ec_strvec_ndup(strvec, 0, 1);
	if (match_strvec == NULL)
		goto fail;

	ec_parsed_tk_set_match(parsed_tk, gen_tk, match_strvec);

	return parsed_tk;

 fail:
	ec_parsed_tk_free(parsed_tk);
	return NULL;
}

static struct ec_tk_ops ec_tk_int_ops = {
	.typename = "int",
	.parse = ec_tk_int_parse,
	.complete = ec_tk_default_complete,
};

struct ec_tk *ec_tk_int(const char *id, long long int min,
	long long int max, unsigned int base)
{
	struct ec_tk *gen_tk = NULL;
	struct ec_tk_int *tk = NULL;

	gen_tk = ec_tk_new(id, &ec_tk_int_ops, sizeof(*tk));
	if (gen_tk == NULL)
		return NULL;
	tk = (struct ec_tk_int *)gen_tk;

	tk->min = min;
	tk->max = max;
	tk->base = base;
	gen_tk->flags |= EC_TK_F_INITIALIZED;

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

static int ec_tk_int_testcase(void)
{
	struct ec_parsed_tk *p;
	struct ec_tk *tk;
	const char *s;
	int ret = 0;

	tk = ec_tk_int(NULL, 0, 256, 0);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "0", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "256", "foo", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "0x100", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, " 1", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "-1", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "0x101", EC_TK_ENDLIST);

	p = ec_tk_parse(tk, "0");
	s = ec_strvec_val(ec_parsed_tk_strvec(p), 0);
	EC_TEST_ASSERT(s != NULL && ec_tk_int_getval(tk, s) == 0);
	ec_parsed_tk_free(p);

	p = ec_tk_parse(tk, "10");
	s = ec_strvec_val(ec_parsed_tk_strvec(p), 0);
	EC_TEST_ASSERT(s != NULL && ec_tk_int_getval(tk, s) == 10);
	ec_parsed_tk_free(p);
	ec_tk_free(tk);

	tk = ec_tk_int(NULL, -1, LLONG_MAX, 16);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "0", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "-1", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "7fffffffffffffff", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "0x7fffffffffffffff", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "-2", EC_TK_ENDLIST);

	p = ec_tk_parse(tk, "10");
	s = ec_strvec_val(ec_parsed_tk_strvec(p), 0);
	EC_TEST_ASSERT(s != NULL && ec_tk_int_getval(tk, s) == 16);
	ec_parsed_tk_free(p);
	ec_tk_free(tk);

	tk = ec_tk_int(NULL, LLONG_MIN, 0, 10);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "0", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "-1", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "-9223372036854775808",
		EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "0x0", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "1", EC_TK_ENDLIST);
	ec_tk_free(tk);

	/* test completion */
	tk = ec_tk_int(NULL, 0, 10, 0);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"", EC_TK_ENDLIST,
		EC_TK_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"x", EC_TK_ENDLIST,
		EC_TK_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"1", EC_TK_ENDLIST,
		EC_TK_ENDLIST,
		"");
	ec_tk_free(tk);

	return ret;
}

static struct ec_test ec_tk_int_test = {
	.name = "tk_int",
	.test = ec_tk_int_testcase,
};

EC_REGISTER_TEST(ec_tk_int_test);
