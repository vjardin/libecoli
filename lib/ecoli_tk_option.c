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

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_strvec.h>
#include <ecoli_tk.h>
#include <ecoli_tk_option.h>
#include <ecoli_tk_str.h>
#include <ecoli_test.h>

struct ec_tk_option {
	struct ec_tk gen;
	struct ec_tk *child;
};

static struct ec_parsed_tk *ec_tk_option_parse(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_option *tk = (struct ec_tk_option *)gen_tk;
	struct ec_parsed_tk *parsed_tk = NULL, *child_parsed_tk;
	struct ec_strvec *match_strvec;

	parsed_tk = ec_parsed_tk_new();
	if (parsed_tk == NULL)
		goto fail;

	child_parsed_tk = ec_tk_parse_tokens(tk->child, strvec);
	if (child_parsed_tk == NULL)
		goto fail;

	if (ec_parsed_tk_matches(child_parsed_tk)) {
		ec_parsed_tk_add_child(parsed_tk, child_parsed_tk);
		match_strvec = ec_strvec_dup(child_parsed_tk->strvec);
	} else {
		ec_parsed_tk_free(child_parsed_tk);
		match_strvec = ec_strvec_new();
	}

	if (match_strvec == NULL)
		goto fail;

	ec_parsed_tk_set_match(parsed_tk, gen_tk, match_strvec);

	return parsed_tk;

 fail:
	ec_parsed_tk_free(parsed_tk);
	return NULL;
}

static struct ec_completed_tk *ec_tk_option_complete(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_option *tk = (struct ec_tk_option *)gen_tk;

	return ec_tk_complete_tokens(tk->child, strvec);
}

static void ec_tk_option_free_priv(struct ec_tk *gen_tk)
{
	struct ec_tk_option *tk = (struct ec_tk_option *)gen_tk;

	ec_tk_free(tk->child);
}

static struct ec_tk_ops ec_tk_option_ops = {
	.typename = "option",
	.parse = ec_tk_option_parse,
	.complete = ec_tk_option_complete,
	.free_priv = ec_tk_option_free_priv,
};

struct ec_tk *ec_tk_option_new(const char *id, struct ec_tk *child)
{
	struct ec_tk_option *tk = NULL;

	if (child == NULL)
		return NULL;

	tk = (struct ec_tk_option *)ec_tk_new(id, &ec_tk_option_ops,
		sizeof(*tk));
	if (tk == NULL) {
		ec_tk_free(child);
		return NULL;
	}

	tk->child = child;

	return &tk->gen;
}

static int ec_tk_option_testcase(void)
{
	struct ec_tk *tk;
	int ret = 0;

	tk = ec_tk_option_new(NULL, ec_tk_str(NULL, "foo"));
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "foo", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "foo", "bar", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 0, "bar", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 0, EC_TK_ENDLIST);
	ec_tk_free(tk);

	/* test completion */
	tk = ec_tk_option_new(NULL, ec_tk_str(NULL, "foo"));
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"", EC_TK_ENDLIST,
		"foo", EC_TK_ENDLIST,
		"foo");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"f", EC_TK_ENDLIST,
		"oo", EC_TK_ENDLIST,
		"oo");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"b", EC_TK_ENDLIST,
		EC_TK_ENDLIST,
		"");
	ec_tk_free(tk);

	return ret;
}

static struct ec_test ec_tk_option_test = {
	.name = "tk_option",
	.test = ec_tk_option_testcase,
};

EC_REGISTER_TEST(ec_tk_option_test);
