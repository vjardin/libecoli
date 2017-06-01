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
#include <errno.h>

#include <ecoli_log.h>
#include <ecoli_malloc.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_tk.h>
#include <ecoli_tk_str.h>

struct ec_tk_str {
	struct ec_tk gen;
	char *string;
	unsigned len;
};

static struct ec_parsed_tk *ec_tk_str_parse(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_str *tk = (struct ec_tk_str *)gen_tk;
	struct ec_strvec *match_strvec;
	struct ec_parsed_tk *parsed_tk = NULL;
	const char *str;

	parsed_tk = ec_parsed_tk_new();
	if (parsed_tk == NULL)
		goto fail;

	if (ec_strvec_len(strvec) == 0)
		return parsed_tk;

	str = ec_strvec_val(strvec, 0);
	if (strcmp(str, tk->string) != 0)
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

static struct ec_completed_tk *ec_tk_str_complete(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_str *tk = (struct ec_tk_str *)gen_tk;
	struct ec_completed_tk *completed_tk;
	struct ec_completed_tk_elt *completed_tk_elt;
	const char *str, *add;
	size_t n = 0;

	completed_tk = ec_completed_tk_new();
	if (completed_tk == NULL)
		return NULL;

	if (ec_strvec_len(strvec) != 1)
		return completed_tk;

	str = ec_strvec_val(strvec, 0);
	for (n = 0; n < tk->len; n++) {
		if (str[n] != tk->string[n])
			break;
	}

	if (str[n] != '\0')
		add = NULL;
	else
		add = tk->string + n;

	completed_tk_elt = ec_completed_tk_elt_new(gen_tk, add);
	if (completed_tk_elt == NULL) {
		ec_completed_tk_free(completed_tk);
		return NULL;
	}

	ec_completed_tk_add_elt(completed_tk, completed_tk_elt);

	return completed_tk;
}

static const char *ec_tk_str_desc(const struct ec_tk *gen_tk)
{
	struct ec_tk_str *tk = (struct ec_tk_str *)gen_tk;

	return tk->string;
}

static void ec_tk_str_free_priv(struct ec_tk *gen_tk)
{
	struct ec_tk_str *tk = (struct ec_tk_str *)gen_tk;

	ec_free(tk->string);
}

static struct ec_tk_type ec_tk_str_type = {
	.name = "str",
	.parse = ec_tk_str_parse,
	.complete = ec_tk_str_complete,
	.desc = ec_tk_str_desc,
	.free_priv = ec_tk_str_free_priv,
};

EC_TK_TYPE_REGISTER(ec_tk_str_type);

struct ec_tk *ec_tk_str_new(const char *id)
{
	struct ec_tk *gen_tk = NULL;

	gen_tk = ec_tk_new(id, &ec_tk_str_type, sizeof(struct ec_tk_str));
	if (gen_tk == NULL)
		return NULL;

	return gen_tk;
}

int ec_tk_str_set_str(struct ec_tk *gen_tk, const char *str)
{
	struct ec_tk_str *tk = (struct ec_tk_str *)gen_tk;

	if (str == NULL)
		return -EINVAL;
	if (tk->string != NULL)
		return -EEXIST; // XXX allow to replace

	tk->string = ec_strdup(str);
	if (tk->string == NULL)
		return -ENOMEM;

	tk->len = strlen(tk->string);

	return 0;
}

struct ec_tk *ec_tk_str(const char *id, const char *str)
{
	struct ec_tk *gen_tk = NULL;

	gen_tk = ec_tk_str_new(id);
	if (gen_tk == NULL)
		goto fail;

	if (ec_tk_str_set_str(gen_tk, str) < 0)
		goto fail;

	return gen_tk;

fail:
	ec_tk_free(gen_tk);
	return NULL;
}

static int ec_tk_str_testcase(void)
{
	struct ec_tk *tk;
	int ret = 0;

	/* XXX use EC_NO_ID instead of NULL */
	tk = ec_tk_str(NULL, "foo");
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "foo");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "foo", "bar");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "foobar");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, " foo");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "");
	ec_tk_free(tk);

	tk = ec_tk_str(NULL, "Здравствуйте");
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "Здравствуйте");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "Здравствуйте",
		"John!");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "foo");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "");
	ec_tk_free(tk);

	/* an empty token string always matches */
	tk = ec_tk_str(NULL, "");
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "", "foo");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "foo");
	ec_tk_free(tk);

	/* test completion */
	tk = ec_tk_str(NULL, "foo");
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		EC_TK_ENDLIST,
		EC_TK_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"", EC_TK_ENDLIST,
		"foo", EC_TK_ENDLIST,
		"foo");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"f", EC_TK_ENDLIST,
		"oo", EC_TK_ENDLIST,
		"oo");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"foo", EC_TK_ENDLIST,
		"", EC_TK_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"x", EC_TK_ENDLIST,
		EC_TK_ENDLIST,
		"");
	ec_tk_free(tk);

	return ret;
}

static struct ec_test ec_tk_str_test = {
	.name = "tk_str",
	.test = ec_tk_str_testcase,
};

EC_TEST_REGISTER(ec_tk_str_test);
