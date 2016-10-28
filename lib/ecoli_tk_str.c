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

#include <ecoli_log.h>
#include <ecoli_malloc.h>
#include <ecoli_test.h>
#include <ecoli_tk.h>
#include <ecoli_tk_str.h>

static struct ec_parsed_tk *parse(const struct ec_tk *gen_tk,
	const char *str)
{
	struct ec_tk_str *tk = (struct ec_tk_str *)gen_tk;
	struct ec_parsed_tk *parsed_tk;

	if (strncmp(str, tk->string, tk->len) != 0)
		return NULL;

	parsed_tk = ec_parsed_tk_new(gen_tk);
	if (parsed_tk == NULL)
		return NULL;

	parsed_tk->str = ec_strndup(str, tk->len);

	return parsed_tk;
}

static struct ec_completed_tk *complete(const struct ec_tk *gen_tk,
	const char *str)
{
	struct ec_tk_str *tk = (struct ec_tk_str *)gen_tk;
	struct ec_completed_tk *completed_tk;
	struct ec_completed_tk_elt *completed_tk_elt;
	size_t n;

	for (n = 0; n < tk->len; n++) {
		if (str[n] != tk->string[n])
			break;
	}

	if (str[n] != '\0')
		return NULL;

	completed_tk = ec_completed_tk_new();
	if (completed_tk == NULL)
		return NULL;

	completed_tk_elt = ec_completed_tk_elt_new(gen_tk, tk->string + n,
		tk->string);
	if (completed_tk_elt == NULL) {
		ec_completed_tk_free(completed_tk);
		return NULL;
	}

	ec_completed_tk_add_elt(completed_tk, completed_tk_elt);

	return completed_tk;
}

static void free_priv(struct ec_tk *gen_tk)
{
	struct ec_tk_str *tk = (struct ec_tk_str *)gen_tk;

	ec_free(tk->string);
}

static struct ec_tk_ops str_ops = {
	.parse = parse,
	.complete = complete,
	.free_priv = free_priv,
};

struct ec_tk *ec_tk_str_new(const char *id, const char *str)
{
	struct ec_tk_str *tk = NULL;
	char *s = NULL;

	tk = (struct ec_tk_str *)ec_tk_new(id, &str_ops, sizeof(*tk));
	if (tk == NULL)
		goto fail;

	s = ec_strdup(str);
	if (s == NULL)
		goto fail;

	tk->string = s;
	tk->len = strlen(s);

	return &tk->gen;

fail:
	ec_free(s);
	ec_free(tk);
	return NULL;
}

static int testcase(void)
{
	struct ec_tk *tk;
	int ret = 0;

	/* all inputs starting with foo should match */
	tk = ec_tk_str_new(NULL, "foo");
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "foo", "foo");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "foobar", "foo");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, " foo", NULL);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "", NULL);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "foo", "foo");
	ec_tk_free(tk);

	/* all inputs starting with foo should match */
	tk = ec_tk_str_new(NULL, "Здравствуйте");
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "Здравствуйте", "Здравствуйте");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "Здравствуйте John!", "Здравствуйте");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "foo", NULL);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "", NULL);
	ec_tk_free(tk);

	/* an empty token string always matches */
	tk = ec_tk_str_new(NULL, "");
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "", "");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "foo", "");
	ec_tk_free(tk);

	/* test completion */
	tk = ec_tk_str_new(NULL, "foo");
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "", "foo");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "f", "oo");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "foo", "");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "x", NULL);
	ec_tk_free(tk);

	return ret;
}

static struct ec_test test = {
	.name = "tk_str",
	.test = testcase,
};

EC_REGISTER_TEST(test);
