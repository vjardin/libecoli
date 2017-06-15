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
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_tk.h>
#include <ecoli_tk_str.h>
#include <ecoli_tk_option.h>
#include <ecoli_tk_seq.h>

struct ec_tk_seq {
	struct ec_tk gen;
	struct ec_tk **table;
	unsigned int len;
};

static struct ec_parsed_tk *ec_tk_seq_parse(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_seq *tk = (struct ec_tk_seq *)gen_tk;
	struct ec_parsed_tk *parsed_tk, *child_parsed_tk;
	struct ec_strvec *match_strvec;
	struct ec_strvec *childvec = NULL;
	size_t len = 0;
	unsigned int i;

	parsed_tk = ec_parsed_tk_new();
	if (parsed_tk == NULL)
		goto fail;

	for (i = 0; i < tk->len; i++) {
		childvec = ec_strvec_ndup(strvec, len,
			ec_strvec_len(strvec) - len);
		if (childvec == NULL)
			goto fail;

		child_parsed_tk = ec_tk_parse_tokens(tk->table[i], childvec);
		if (child_parsed_tk == NULL)
			goto fail;

		ec_strvec_free(childvec);
		childvec = NULL;

		if (!ec_parsed_tk_matches(child_parsed_tk)) {
			ec_parsed_tk_free(child_parsed_tk);
			// XXX ec_parsed_tk_free_children needed? see subset.c
			ec_parsed_tk_free_children(parsed_tk);
			return parsed_tk;
		}

		ec_parsed_tk_add_child(parsed_tk, child_parsed_tk);
		len += ec_parsed_tk_len(child_parsed_tk);
	}

	match_strvec = ec_strvec_ndup(strvec, 0, len);
	if (match_strvec == NULL)
		goto fail;

	ec_parsed_tk_set_match(parsed_tk, gen_tk, match_strvec);

	return parsed_tk;

fail:
	ec_strvec_free(childvec);
	ec_parsed_tk_free(parsed_tk);
	return NULL;
}

static struct ec_completed_tk *ec_tk_seq_complete(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_seq *tk = (struct ec_tk_seq *)gen_tk;
	struct ec_completed_tk *completed_tk, *child_completed_tk;
	struct ec_strvec *childvec = NULL;
	struct ec_parsed_tk *parsed_tk;
	size_t len = 0;
	unsigned int i;

	completed_tk = ec_completed_tk_new();
	if (completed_tk == NULL)
		return NULL;

	if (tk->len == 0)
		return completed_tk;

	for (i = 0; i < tk->len && len < ec_strvec_len(strvec); i++) {
		childvec = ec_strvec_ndup(strvec, len,
			ec_strvec_len(strvec) - len);
		if (childvec == NULL)
			goto fail;

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

static void ec_tk_seq_free_priv(struct ec_tk *gen_tk)
{
	struct ec_tk_seq *tk = (struct ec_tk_seq *)gen_tk;
	unsigned int i;

	for (i = 0; i < tk->len; i++)
		ec_tk_free(tk->table[i]);
	ec_free(tk->table);
}

static struct ec_tk_type ec_tk_seq_type = {
	.name = "seq",
	.parse = ec_tk_seq_parse,
	.complete = ec_tk_seq_complete,
	.size = sizeof(struct ec_tk_seq),
	.free_priv = ec_tk_seq_free_priv,
};

EC_TK_TYPE_REGISTER(ec_tk_seq_type);

int ec_tk_seq_add(struct ec_tk *gen_tk, struct ec_tk *child)
{
	struct ec_tk_seq *tk = (struct ec_tk_seq *)gen_tk;
	struct ec_tk **table;

	// XXX check tk type

	assert(tk != NULL);

	if (child == NULL)
		return -EINVAL;

	gen_tk->flags &= ~EC_TK_F_BUILT;

	table = ec_realloc(tk->table, (tk->len + 1) * sizeof(*tk->table));
	if (table == NULL) {
		ec_tk_free(child);
		return -1;
	}

	tk->table = table;
	table[tk->len] = child;
	tk->len++;

	child->parent = gen_tk;
	TAILQ_INSERT_TAIL(&gen_tk->children, child, next); // XXX really needed?

	return 0;
}

struct ec_tk *__ec_tk_seq(const char *id, ...)
{
	struct ec_tk *gen_tk = NULL;
	struct ec_tk_seq *tk = NULL;
	struct ec_tk *child;
	va_list ap;
	int fail = 0;

	va_start(ap, id);

	gen_tk = __ec_tk_new(&ec_tk_seq_type, id);
	tk = (struct ec_tk_seq *)gen_tk;
	if (tk == NULL)
		fail = 1;;

	for (child = va_arg(ap, struct ec_tk *);
	     child != EC_TK_ENDLIST;
	     child = va_arg(ap, struct ec_tk *)) {

		/* on error, don't quit the loop to avoid leaks */
		if (fail == 1 || child == NULL ||
				ec_tk_seq_add(&tk->gen, child) < 0) {
			fail = 1;
			ec_tk_free(child);
		}
	}

	if (fail == 1)
		goto fail;

	va_end(ap);
	return gen_tk;

fail:
	ec_tk_free(gen_tk); /* will also free children */
	va_end(ap);
	return NULL;
}

static int ec_tk_seq_testcase(void)
{
	struct ec_tk *tk;
	int ret = 0;

	tk = EC_TK_SEQ(NULL,
		ec_tk_str(NULL, "foo"),
		ec_tk_str(NULL, "bar")
	);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 2, "foo", "bar");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 2, "foo", "bar", "toto");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "foo");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "foox", "bar");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "foo", "barx");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "bar", "foo");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "", "foo");
	ec_tk_free(tk);

	/* test completion */
	tk = EC_TK_SEQ(NULL,
		ec_tk_str(NULL, "foo"),
		ec_tk_option_new(NULL, ec_tk_str(NULL, "toto")),
		ec_tk_str(NULL, "bar")
	);
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
		"foo", EC_TK_ENDLIST,
		"", EC_TK_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"foo", "", EC_TK_ENDLIST,
		"bar", "toto", EC_TK_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"foo", "t", EC_TK_ENDLIST,
		"oto", EC_TK_ENDLIST,
		"oto");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"foo", "b", EC_TK_ENDLIST,
		"ar", EC_TK_ENDLIST,
		"ar");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"foo", "bar", EC_TK_ENDLIST,
		"", EC_TK_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"x", EC_TK_ENDLIST,
		EC_TK_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"foobarx", EC_TK_ENDLIST,
		EC_TK_ENDLIST,
		"");
	ec_tk_free(tk);

	return ret;
}

static struct ec_test ec_tk_seq_test = {
	.name = "tk_seq",
	.test = ec_tk_seq_testcase,
};

EC_TEST_REGISTER(ec_tk_seq_test);
