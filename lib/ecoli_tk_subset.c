/*
 * Copyright (c) 2016-2017, Olivier MATZ <zer0@droids-corp.org>
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
#include <stdbool.h>

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_strvec.h>
#include <ecoli_tk.h>
#include <ecoli_tk_subset.h>
#include <ecoli_tk_str.h>
#include <ecoli_tk_or.h>
#include <ecoli_test.h>

struct ec_tk_subset {
	struct ec_tk gen;
	struct ec_tk **table;
	unsigned int len;
};

struct parse_result {
	struct ec_parsed_tk **parsed_table; /* list of parsed tk */
	size_t parsed_table_len;            /* number of parsed tk */
	size_t len;                         /* consumed strings */
};

static int __ec_tk_subset_parse(struct ec_tk **table, size_t table_len,
				const struct ec_strvec *strvec,
				struct parse_result *out)
{
	struct ec_tk **child_table;
	struct ec_parsed_tk *child_parsed_tk = NULL;
	struct ec_strvec *childvec = NULL;
	size_t i, j, len = 0;
	struct parse_result best_result, result;
	int ret;

	if (table_len == 0)
		return 0;

	memset(&best_result, 0, sizeof(best_result));
	best_result.parsed_table =
		ec_calloc(table_len, sizeof(*best_result.parsed_table[0]));
	if (best_result.parsed_table == NULL)
		goto fail;

	child_table = ec_calloc(table_len - 1, sizeof(*child_table));
	if (child_table == NULL)
		goto fail;

	for (i = 0; i < table_len; i++) {
		/* try to parse elt i */
		child_parsed_tk = ec_tk_parse_tokens(table[i], strvec);
		if (child_parsed_tk == NULL)
			goto fail;

		if (!ec_parsed_tk_matches(child_parsed_tk)) {
			ec_parsed_tk_free(child_parsed_tk);
			child_parsed_tk = NULL;
			continue;
		}

		/* build a new table without elt i */
		for (j = 0; j < table_len; j++) {
			if (j < i)
				child_table[j] = table[j];
			else if (j > i)
				child_table[j - 1] = table[j];
		}

		/* build a new strvec */
		len = ec_parsed_tk_len(child_parsed_tk);
		childvec = ec_strvec_ndup(strvec, len,
					ec_strvec_len(strvec) - len);
		if (childvec == NULL)
			goto fail;

		memset(&result, 0, sizeof(result));

		ret = __ec_tk_subset_parse(child_table, table_len - 1,
					childvec, &result);
		ec_strvec_free(childvec);
		childvec = NULL;
		if (ret < 0)
			goto fail;

		/* if result is not the best, ignore */
		if (result.parsed_table_len + 1 <=
				best_result.parsed_table_len) {
			ec_parsed_tk_free(child_parsed_tk);
			child_parsed_tk = NULL;
			for (j = 0; j < result.parsed_table_len; j++)
				ec_parsed_tk_free(result.parsed_table[j]);
			ec_free(result.parsed_table);
			memset(&result, 0, sizeof(result));
			continue;
		}

		/* replace the previous best result */
		for (j = 0; j < best_result.parsed_table_len; j++)
			ec_parsed_tk_free(best_result.parsed_table[j]);
		best_result.parsed_table[0] = child_parsed_tk;
		child_parsed_tk = NULL;
		for (j = 0; j < result.parsed_table_len; j++)
			best_result.parsed_table[j+1] = result.parsed_table[j];
		best_result.parsed_table_len = result.parsed_table_len + 1;
		best_result.len = len + result.len;
		ec_free(result.parsed_table);
	}

	*out = best_result;
	ec_free(child_table);

	return 0;

 fail:
	ec_parsed_tk_free(child_parsed_tk);
	ec_strvec_free(childvec);
	for (j = 0; j < best_result.parsed_table_len; j++)
		ec_parsed_tk_free(best_result.parsed_table[j]);
	ec_free(best_result.parsed_table);
	ec_free(child_table);
	return -1;
}

static struct ec_parsed_tk *ec_tk_subset_parse(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_subset *tk = (struct ec_tk_subset *)gen_tk;
	struct ec_parsed_tk *parsed_tk = NULL;
	struct parse_result result;
	struct ec_strvec *match_strvec;
	size_t i;
	int ret;

	memset(&result, 0, sizeof(result));

	parsed_tk = ec_parsed_tk_new();
	if (parsed_tk == NULL)
		goto fail;

	ret = __ec_tk_subset_parse(tk->table, tk->len, strvec, &result);
	if (ret < 0)
		goto fail;

	if (result.parsed_table_len == 0) {
		ec_free(result.parsed_table);
		return parsed_tk;
	}

	for (i = 0; i < result.parsed_table_len; i++) {
		ec_parsed_tk_add_child(parsed_tk, result.parsed_table[i]);
		result.parsed_table[i] = NULL;
	}
	ec_free(result.parsed_table);
	result.parsed_table = NULL;

	match_strvec = ec_strvec_ndup(strvec, 0, result.len);
	if (match_strvec == NULL)
		goto fail;

	ec_parsed_tk_set_match(parsed_tk, gen_tk, match_strvec);

	return parsed_tk;

 fail:
	for (i = 0; i < result.parsed_table_len; i++)
		ec_parsed_tk_free(result.parsed_table[i]);
	ec_free(result.parsed_table);
	ec_parsed_tk_free(parsed_tk);

	return NULL;
}

static struct ec_completed_tk *
__ec_tk_subset_complete(struct ec_tk **table, size_t table_len,
	const struct ec_strvec *strvec)
{
	struct ec_completed_tk *completed_tk = NULL;
	struct ec_completed_tk *child_completed_tk = NULL;
	struct ec_strvec *childvec = NULL;
	struct ec_parsed_tk *parsed_tk = NULL;
	struct ec_tk *save;
	size_t i, len;

	/*
	 * example with table = [a, b, c]
	 * subset_complete([a,b,c], strvec) returns:
	 *   complete(a, strvec) + complete(b, strvec) + complete(c, strvec) +
	 *   + __subset_complete([b, c], childvec) if a matches
	 *   + __subset_complete([a, c], childvec) if b matches
	 *   + __subset_complete([a, b], childvec) if c matches
	 */

	completed_tk = ec_completed_tk_new();
	if (completed_tk == NULL)
		goto fail;

	/* first, try to complete with each token of the table */
	for (i = 0; i < table_len; i++) {
		if (table[i] == NULL)
			continue;

		child_completed_tk = ec_tk_complete_tokens(table[i],
			strvec);

		if (child_completed_tk == NULL)
			goto fail;

		ec_completed_tk_merge(completed_tk, child_completed_tk);
		child_completed_tk = NULL;
	}

	/* then, if a token matches, advance in strvec and try to complete with
	 * all the other tokens */
	for (i = 0; i < table_len; i++) {
		if (table[i] == NULL)
			continue;

		parsed_tk = ec_tk_parse_tokens(table[i], strvec);
		if (parsed_tk == NULL)
			goto fail;

		if (!ec_parsed_tk_matches(parsed_tk)) {
			ec_parsed_tk_free(parsed_tk);
			parsed_tk = NULL;
			continue;
		}

		len = ec_parsed_tk_len(parsed_tk);
		childvec = ec_strvec_ndup(strvec, len,
					ec_strvec_len(strvec) - len);
		if (childvec == NULL)
			goto fail;

		save = table[i];
		table[i] = NULL;
		child_completed_tk = __ec_tk_subset_complete(table,
							table_len, childvec);
		table[i] = save;
		ec_strvec_free(childvec);
		childvec = NULL;

		if (child_completed_tk == NULL)
			goto fail;

		ec_completed_tk_merge(completed_tk, child_completed_tk);
		child_completed_tk = NULL;

		ec_parsed_tk_free(parsed_tk);
		parsed_tk = NULL;
	}

	return completed_tk;
fail:
	ec_parsed_tk_free(parsed_tk);
	ec_completed_tk_free(child_completed_tk);
	ec_completed_tk_free(completed_tk);

	return NULL;
}

static struct ec_completed_tk *ec_tk_subset_complete(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_subset *tk = (struct ec_tk_subset *)gen_tk;

	return __ec_tk_subset_complete(tk->table, tk->len, strvec);
}

static void ec_tk_subset_free_priv(struct ec_tk *gen_tk)
{
	struct ec_tk_subset *tk = (struct ec_tk_subset *)gen_tk;
	unsigned int i;

	for (i = 0; i < tk->len; i++)
		ec_tk_free(tk->table[i]);
	ec_free(tk->table);
}

int ec_tk_subset_add(struct ec_tk *gen_tk, struct ec_tk *child)
{
	struct ec_tk_subset *tk = (struct ec_tk_subset *)gen_tk;
	struct ec_tk **table;

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
	TAILQ_INSERT_TAIL(&gen_tk->children, child, next);

	return 0;
}

static struct ec_tk_type ec_tk_subset_type = {
	.name = "tk_subset",
	.parse = ec_tk_subset_parse,
	.complete = ec_tk_subset_complete,
	.free_priv = ec_tk_subset_free_priv,
};

EC_TK_TYPE_REGISTER(ec_tk_subset_type);

struct ec_tk *ec_tk_subset(const char *id)
{
	struct ec_tk *gen_tk = NULL;
	struct ec_tk_subset *tk = NULL;

	gen_tk = ec_tk_new(id, &ec_tk_subset_type, sizeof(*tk));
	if (gen_tk == NULL)
		return NULL;

	tk = (struct ec_tk_subset *)gen_tk;
	tk->table = NULL;
	tk->len = 0;

	return gen_tk;
}

struct ec_tk *__ec_tk_subset(const char *id, ...)
{
	struct ec_tk *gen_tk = NULL;
	struct ec_tk_subset *tk = NULL;
	struct ec_tk *child;
	va_list ap;
	int fail = 0;

	va_start(ap, id);

	gen_tk = ec_tk_subset(id);
	tk = (struct ec_tk_subset *)gen_tk;
	if (tk == NULL)
		fail = 1;;

	for (child = va_arg(ap, struct ec_tk *);
	     child != EC_TK_ENDLIST;
	     child = va_arg(ap, struct ec_tk *)) {

		/* on error, don't quit the loop to avoid leaks */
		if (fail == 1 || child == NULL ||
				ec_tk_subset_add(gen_tk, child) < 0) {
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

static int ec_tk_subset_testcase(void)
{
	struct ec_tk *tk;
	int ret = 0;

	tk = EC_TK_SUBSET(NULL,
		EC_TK_OR(NULL,
			ec_tk_str(NULL, "foo"),
			ec_tk_str(NULL, "bar")),
		ec_tk_str(NULL, "bar"),
		ec_tk_str(NULL, "toto")
	);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "foo");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "bar");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 2, "foo", "bar", "titi");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 3, "bar", "foo", "toto");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "foo", "foo");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 2, "bar", "bar");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 2, "bar", "foo");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, " ");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "foox");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "titi");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "");
	ec_tk_free(tk);

	/* test completion */
	tk = EC_TK_SUBSET(NULL,
		ec_tk_str(NULL, "foo"),
		ec_tk_str(NULL, "bar"),
		ec_tk_str(NULL, "bar2"),
		ec_tk_str(NULL, "toto"),
		ec_tk_str(NULL, "titi")
	);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"", EC_TK_ENDLIST,
		"foo", "bar", "bar2", "toto", "titi", EC_TK_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"bar", "bar2", "", EC_TK_ENDLIST,
		"foo", "toto", "titi", EC_TK_ENDLIST,
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
		"bar", "b", EC_TK_ENDLIST,
		"ar2", EC_TK_ENDLIST,
		"ar2");
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

static struct ec_test ec_tk_subset_test = {
	.name = "tk_subset",
	.test = ec_tk_subset_testcase,
};

EC_TEST_REGISTER(ec_tk_subset_test);
