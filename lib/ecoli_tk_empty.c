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

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_tk.h>
#include <ecoli_tk_empty.h>

static struct ec_parsed_tk *ec_tk_empty_parse(const struct ec_tk *gen_tk,
	const char *str)
{
	struct ec_parsed_tk *parsed_tk;

	parsed_tk = ec_parsed_tk_new(gen_tk);
	if (parsed_tk == NULL)
		return NULL;

	(void)str;
	parsed_tk->str = ec_strdup("");

	return parsed_tk;
}

static struct ec_tk_ops ec_tk_empty_ops = {
	.parse = ec_tk_empty_parse,
};

struct ec_tk *ec_tk_empty_new(const char *id)
{
	return ec_tk_new(id, &ec_tk_empty_ops, sizeof(struct ec_tk_empty));
}

static int ec_tk_empty_testcase(void)
{
	struct ec_tk *tk;
	int ret = 0;

	/* all inputs match */
	tk = ec_tk_empty_new(NULL);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "foo", "");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, " foo", "");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "", "");
	ec_tk_free(tk);

	/* never completes */
	tk = ec_tk_empty_new(NULL);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "", NULL);
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "foo", NULL);
	ec_tk_free(tk);

	return ret;
}

static struct ec_test ec_tk_empty_test = {
	.name = "tk_empty",
	.test = ec_tk_empty_testcase,
};

EC_REGISTER_TEST(ec_tk_empty_test);
