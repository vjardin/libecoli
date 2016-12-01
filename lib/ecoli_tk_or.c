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
#include <ecoli_tk_or.h>
#include <ecoli_tk_str.h>
#include <ecoli_test.h>

struct ec_tk_or {
	struct ec_tk gen;
	struct ec_tk **table;
	unsigned int len;
};

static struct ec_parsed_tk *ec_tk_or_parse(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_or *tk = (struct ec_tk_or *)gen_tk;
	struct ec_parsed_tk *parsed_tk, *child_parsed_tk = NULL;
	struct ec_strvec *match_strvec;
	unsigned int i;

	parsed_tk = ec_parsed_tk_new();
	if (parsed_tk == NULL)
		goto fail;

	for (i = 0; i < tk->len; i++) {
		child_parsed_tk = ec_tk_parse_tokens(tk->table[i], strvec);
		if (child_parsed_tk == NULL)
			goto fail;
		if (ec_parsed_tk_matches(child_parsed_tk))
			break;
		ec_parsed_tk_free(child_parsed_tk);
		child_parsed_tk = NULL;
	}

	/* no match */
	if (i == tk->len)
		return parsed_tk;

	match_strvec = ec_strvec_dup(child_parsed_tk->strvec);
	if (match_strvec == NULL)
		goto fail;

	ec_parsed_tk_set_match(parsed_tk, gen_tk, match_strvec);
	ec_parsed_tk_add_child(parsed_tk, child_parsed_tk);

	return parsed_tk;

 fail:
	ec_parsed_tk_free(child_parsed_tk);
	ec_parsed_tk_free(parsed_tk);
	return NULL;
}

static struct ec_completed_tk *ec_tk_or_complete(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_or *tk = (struct ec_tk_or *)gen_tk;
	struct ec_completed_tk *completed_tk, *child_completed_tk;
	size_t n;

	completed_tk = ec_completed_tk_new();
	if (completed_tk == NULL)
		return NULL;

	for (n = 0; n < tk->len; n++) {
		child_completed_tk = ec_tk_complete_tokens(tk->table[n],
			strvec);

		if (child_completed_tk == NULL) // XXX fail instead?
			continue;

		ec_completed_tk_merge(completed_tk, child_completed_tk);
	}

	return completed_tk;
}

static void ec_tk_or_free_priv(struct ec_tk *gen_tk)
{
	struct ec_tk_or *tk = (struct ec_tk_or *)gen_tk;
	unsigned int i;

	for (i = 0; i < tk->len; i++)
		ec_tk_free(tk->table[i]);
	ec_free(tk->table);
}

static struct ec_tk_ops ec_tk_or_ops = {
	.typename = "or",
	.parse = ec_tk_or_parse,
	.complete = ec_tk_or_complete,
	.free_priv = ec_tk_or_free_priv,
};

struct ec_tk *ec_tk_or_new(const char *id)
{
	struct ec_tk_or *tk = NULL;

	tk = (struct ec_tk_or *)ec_tk_new(id, &ec_tk_or_ops, sizeof(*tk));
	if (tk == NULL)
		return NULL;

	tk->table = NULL;
	tk->len = 0;

	return &tk->gen;
}

struct ec_tk *ec_tk_or_new_list(const char *id, ...)
{
	struct ec_tk_or *tk = NULL;
	struct ec_tk *child;
	va_list ap;
	int fail = 0;

	va_start(ap, id);

	tk = (struct ec_tk_or *)ec_tk_or_new(id);
	if (tk == NULL)
		goto fail;

	for (child = va_arg(ap, struct ec_tk *);
	     child != EC_TK_ENDLIST;
	     child = va_arg(ap, struct ec_tk *)) {
		/* on error, don't quit the loop to avoid leaks */

		if (child == NULL || ec_tk_or_add(&tk->gen, child) < 0)
			fail = 1;
	}

	if (fail == 1)
		goto fail;

	va_end(ap);
	return &tk->gen;

fail:
	ec_tk_free(&tk->gen); /* will also free children */
	va_end(ap);
	return NULL;
}

int ec_tk_or_add(struct ec_tk *gen_tk, struct ec_tk *child)
{
	struct ec_tk_or *tk = (struct ec_tk_or *)gen_tk;
	struct ec_tk **table;

	assert(tk != NULL);
	assert(child != NULL);

	table = ec_realloc(tk->table, (tk->len + 1) * sizeof(*tk->table));
	if (table == NULL)
		return -1;

	tk->table = table;
	table[tk->len] = child;
	tk->len ++;

	child->parent = gen_tk;
	TAILQ_INSERT_TAIL(&gen_tk->children, child, next);

	return 0;
}

static int ec_tk_or_testcase(void)
{
	struct ec_tk *tk;
	int ret = 0;

	tk = ec_tk_or_new_list(NULL,
		ec_tk_str_new(NULL, "foo"),
		ec_tk_str_new(NULL, "bar"),
		EC_TK_ENDLIST);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "foo", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "bar", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "foo", "bar", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, " ", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "foox", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "toto", EC_TK_ENDLIST);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "", EC_TK_ENDLIST);
	ec_tk_free(tk);

	/* test completion */
	tk = ec_tk_or_new_list(NULL,
		ec_tk_str_new(NULL, "foo"),
		ec_tk_str_new(NULL, "bar"),
		ec_tk_str_new(NULL, "bar2"),
		ec_tk_str_new(NULL, "toto"),
		ec_tk_str_new(NULL, "titi"),
		EC_TK_ENDLIST);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"", EC_TK_ENDLIST,
		"foo", "bar", "bar2", "toto", "titi", EC_TK_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"f", EC_TK_ENDLIST,
		"oo", EC_TK_ENDLIST,
		"oo");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"b", EC_TK_ENDLIST,
		"ar", "ar2", EC_TK_ENDLIST,
		"ar");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"bar", EC_TK_ENDLIST,
		"", "2", EC_TK_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"t", EC_TK_ENDLIST,
		"oto", "iti", EC_TK_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"to", EC_TK_ENDLIST,
		"to", EC_TK_ENDLIST,
		"to");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"x", EC_TK_ENDLIST,
		EC_TK_ENDLIST,
		"");
	ec_tk_free(tk);

	return ret;
}

static struct ec_test ec_tk_or_test = {
	.name = "tk_or",
	.test = ec_tk_or_testcase,
};

EC_REGISTER_TEST(ec_tk_or_test);
