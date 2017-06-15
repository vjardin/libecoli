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
#include <regex.h>

#include <ecoli_log.h>
#include <ecoli_malloc.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_tk.h>
#include <ecoli_tk_re.h>

struct ec_tk_re {
	struct ec_tk gen;
	char *re_str;
	regex_t re;
};

static struct ec_parsed_tk *ec_tk_re_parse(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_re *tk = (struct ec_tk_re *)gen_tk;
	struct ec_strvec *match_strvec;
	struct ec_parsed_tk *parsed_tk = NULL;
	const char *str;
	regmatch_t pos;

	parsed_tk = ec_parsed_tk_new();
	if (parsed_tk == NULL)
		goto fail;

	if (ec_strvec_len(strvec) == 0)
		return parsed_tk;

	str = ec_strvec_val(strvec, 0);
	if (regexec(&tk->re, str, 1, &pos, 0) != 0)
		return parsed_tk;
	if (pos.rm_so != 0 || pos.rm_eo != (int)strlen(str))
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

static void ec_tk_re_free_priv(struct ec_tk *gen_tk)
{
	struct ec_tk_re *tk = (struct ec_tk_re *)gen_tk;

	ec_free(tk->re_str);
	regfree(&tk->re);
}

static struct ec_tk_type ec_tk_re_type = {
	.name = "re",
	.parse = ec_tk_re_parse,
	.complete = ec_tk_default_complete,
	.size = sizeof(struct ec_tk_re),
	.free_priv = ec_tk_re_free_priv,
};

EC_TK_TYPE_REGISTER(ec_tk_re_type);

int ec_tk_re_set_regexp(struct ec_tk *gen_tk, const char *str)
{
	struct ec_tk_re *tk = (struct ec_tk_re *)gen_tk;
	int ret;

	if (str == NULL)
		return -EINVAL;
	if (tk->re_str != NULL) // XXX allow to replace
		return -EEXIST;

	ret = -ENOMEM;
	tk->re_str = ec_strdup(str);
	if (tk->re_str == NULL)
		goto fail;

	ret = -EINVAL;
	if (regcomp(&tk->re, tk->re_str, REG_EXTENDED) != 0)
		goto fail;

	return 0;

fail:
	ec_free(tk->re_str);
	return ret;
}

struct ec_tk *ec_tk_re(const char *id, const char *re_str)
{
	struct ec_tk *gen_tk = NULL;

	gen_tk = __ec_tk_new(&ec_tk_re_type, id);
	if (gen_tk == NULL)
		goto fail;

	if (ec_tk_re_set_regexp(gen_tk, re_str) < 0)
		goto fail;

	return gen_tk;

fail:
	ec_tk_free(gen_tk);
	return NULL;
}

static int ec_tk_re_testcase(void)
{
	struct ec_tk *tk;
	int ret = 0;

	tk = ec_tk_re(NULL, "fo+|bar");
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "foo");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "foo", "bar");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "bar");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "foobar");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, " foo");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "");
	ec_tk_free(tk);

	return ret;
}

static struct ec_test ec_tk_re_test = {
	.name = "tk_re",
	.test = ec_tk_re_testcase,
};

EC_TEST_REGISTER(ec_tk_re_test);
