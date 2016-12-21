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
#include <ctype.h>

#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_malloc.h>
#include <ecoli_strvec.h>
#include <ecoli_tk.h>
#include <ecoli_tk_space.h>

struct ec_tk_space {
	struct ec_tk gen;
};

static struct ec_parsed_tk *ec_tk_space_parse(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_parsed_tk *parsed_tk = NULL;
	struct ec_strvec *match_strvec;
	const char *str;
	size_t len = 0;

	parsed_tk = ec_parsed_tk_new();
	if (parsed_tk == NULL)
		goto fail;

	if (ec_strvec_len(strvec) == 0)
		return parsed_tk;

	str = ec_strvec_val(strvec, 0);
	while (isspace(str[len]))
		len++;
	if (len == 0 || len != strlen(str))
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

static struct ec_tk_ops ec_tk_space_ops = {
	.typename = "space",
	.parse = ec_tk_space_parse,
	.complete = ec_tk_default_complete,
};

struct ec_tk *ec_tk_space_new(const char *id)
{
	return ec_tk_new(id, &ec_tk_space_ops, sizeof(struct ec_tk_space));
}

static int ec_tk_space_testcase(void)
{
	struct ec_tk *tk;
	int ret = 0;

	tk = ec_tk_space_new(NULL);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, " ", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, " ", "foo", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, " foo", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "foo ", EC_TK_ENDLIST);
	ec_tk_free(tk);

	/* test completion */
	tk = ec_tk_space_new(NULL);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"", EC_TK_ENDLIST,
		EC_TK_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		" ", EC_TK_ENDLIST,
		EC_TK_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"foo", EC_TK_ENDLIST,
		EC_TK_ENDLIST,
		"");
	ec_tk_free(tk);

	return ret;
}

static struct ec_test ec_tk_space_test = {
	.name = "tk_space",
	.test = ec_tk_space_testcase,
};

EC_REGISTER_TEST(ec_tk_space_test);
