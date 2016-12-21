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
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_tk.h>
#include <ecoli_tk_str.h>
#include <ecoli_tk_option.h>
#include <ecoli_tk_many.h>

struct ec_tk_many {
	struct ec_tk gen;
	unsigned int min;
	unsigned int max;
	struct ec_tk *child;
};

static struct ec_parsed_tk *ec_tk_many_parse(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_many *tk = (struct ec_tk_many *)gen_tk;
	struct ec_parsed_tk *parsed_tk, *child_parsed_tk;
	struct ec_strvec *match_strvec;
	struct ec_strvec *childvec = NULL;
	size_t off = 0, len, count;

	parsed_tk = ec_parsed_tk_new();
	if (parsed_tk == NULL)
		goto fail;

	for (count = 0; tk->max == 0 || count < tk->max; count++) {
		childvec = ec_strvec_ndup(strvec, off,
			ec_strvec_len(strvec) - off);
		if (childvec == NULL)
			goto fail;

		child_parsed_tk = ec_tk_parse_tokens(tk->child, childvec);
		if (child_parsed_tk == NULL)
			goto fail;

		ec_strvec_free(childvec);
		childvec = NULL;

		if (!ec_parsed_tk_matches(child_parsed_tk)) {
			ec_parsed_tk_free(child_parsed_tk);
			break;
		}

		ec_parsed_tk_add_child(parsed_tk, child_parsed_tk);

		/* it matches "no token", no need to continue */
		len = ec_parsed_tk_len(child_parsed_tk);
		if (len == 0) {
			ec_parsed_tk_free(child_parsed_tk);
			break;
		}

		off += len;
	}

	if (count < tk->min) {
		ec_parsed_tk_free_children(parsed_tk);
		return parsed_tk;
	}

	match_strvec = ec_strvec_ndup(strvec, 0, off);
	if (match_strvec == NULL)
		goto fail;

	ec_parsed_tk_set_match(parsed_tk, gen_tk, match_strvec);

	return parsed_tk;

fail:
	ec_strvec_free(childvec);
	ec_parsed_tk_free(parsed_tk);
	return NULL;
}

#if 0
static struct ec_completed_tk *ec_tk_many_complete(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_many *tk = (struct ec_tk_many *)gen_tk;
	struct ec_completed_tk *completed_tk, *child_completed_tk;
	struct ec_strvec *childvec;
	struct ec_parsed_tk *parsed_tk;
	size_t len = 0;
	unsigned int i;

	completed_tk = ec_completed_tk_new();
	if (completed_tk == NULL)
		return NULL;

	if (tk->len == 0)
		return completed_tk;

	for (i = 0; i < tk->len; i++) {
		childvec = ec_strvec_ndup(strvec, len,
			ec_strvec_len(strvec) - len);
		if (childvec == NULL)
			goto fail; // XXX fail ?

		child_completed_tk = ec_tk_complete_tokens(tk->table[i],
			childvec);
		if (child_completed_tk == NULL)
			goto fail;

		ec_completed_tk_merge(completed_tk, child_completed_tk);

		parsed_tk = ec_tk_parse_tokens(tk->table[i], childvec);
		if (parsed_tk == NULL)
			goto fail;

		ec_strvec_free(childvec);
		childvec = NULL;

		if (!ec_parsed_tk_matches(parsed_tk)) {
			ec_parsed_tk_free(parsed_tk);
			break;
		}

		len += ec_strvec_len(parsed_tk->strvec);
		ec_parsed_tk_free(parsed_tk);
	}

	return completed_tk;

fail:
	ec_strvec_free(childvec);
	ec_completed_tk_free(completed_tk);
	return NULL;
}
#endif

static void ec_tk_many_free_priv(struct ec_tk *gen_tk)
{
	struct ec_tk_many *tk = (struct ec_tk_many *)gen_tk;

	ec_tk_free(tk->child);
}

static struct ec_tk_ops ec_tk_many_ops = {
	.typename = "many",
	.parse = ec_tk_many_parse,
	.complete = ec_tk_default_complete,
//XXX	.complete = ec_tk_many_complete,
	.free_priv = ec_tk_many_free_priv,
};

struct ec_tk *ec_tk_many_new(const char *id, struct ec_tk *child,
	unsigned int min, unsigned int max)
{
	struct ec_tk_many *tk = NULL;

	if (child == NULL)
		return NULL;

	tk = (struct ec_tk_many *)ec_tk_new(id, &ec_tk_many_ops,
		sizeof(*tk));
	if (tk == NULL) {
		ec_tk_free(child);
		return NULL;
	}

	tk->child = child;
	tk->min = min;
	tk->max = max;

	return &tk->gen;
}

static int ec_tk_many_testcase(void)
{
	struct ec_tk *tk;
	int ret = 0;

	tk = ec_tk_many_new(NULL, ec_tk_str(NULL, "foo"), 0, 0);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 0, "bar", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "foo", "bar", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 2, "foo", "foo", "bar",
		EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 0, EC_TK_ENDLIST);
	ec_tk_free(tk);

	tk = ec_tk_many_new(NULL, ec_tk_str(NULL, "foo"), 1, 0);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "bar", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "foo", "bar", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 2, "foo", "foo", "bar",
		EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, EC_TK_ENDLIST);
	ec_tk_free(tk);

	tk = ec_tk_many_new(NULL, ec_tk_str(NULL, "foo"), 1, 2);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "bar", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "foo", "bar", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 2, "foo", "foo", "bar",
		EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 2, "foo", "foo", "foo",
		EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, EC_TK_ENDLIST);
	ec_tk_free(tk);

	/* test completion */
	/* XXX */

	return ret;
}

static struct ec_test ec_tk_many_test = {
	.name = "many",
	.test = ec_tk_many_testcase,
};

EC_REGISTER_TEST(ec_tk_many_test);
