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
#include <ecoli_tk.h>
#include <ecoli_tk_str.h>
#include <ecoli_tk_seq.h>

// XXX to handle the quote, it will be done in tk_shseq
// it will unquote the string and parse each token separately
static struct ec_parsed_tk *ec_tk_seq_parse(const struct ec_tk *gen_tk,
	const char *str)
{
	struct ec_tk_seq *tk = (struct ec_tk_seq *)gen_tk;
	struct ec_parsed_tk *parsed_tk, *child_parsed_tk;
	size_t len = 0;
	unsigned int i;

	parsed_tk = ec_parsed_tk_new(gen_tk);
	if (parsed_tk == NULL)
		return NULL;

	for (i = 0; i < tk->len; i++) {
		child_parsed_tk = ec_tk_parse(tk->table[i], str + len);
		if (child_parsed_tk == NULL)
			goto fail;

		len += strlen(child_parsed_tk->str);
		ec_parsed_tk_add_child(parsed_tk, child_parsed_tk);
	}

	parsed_tk->str = ec_strndup(str, len);

	return parsed_tk;

 fail:
	ec_parsed_tk_free(parsed_tk);
	return NULL;
}

static struct ec_completed_tk *ec_tk_seq_complete(const struct ec_tk *gen_tk,
	const char *str)
{
	struct ec_tk_seq *tk = (struct ec_tk_seq *)gen_tk;
	struct ec_completed_tk *completed_tk;
	struct ec_parsed_tk *parsed_tk;
	size_t len = 0;
	unsigned int i;

	if (tk->len == 0)
		return ec_completed_tk_new();

	/* parse the first tokens */
	for (i = 0; i < tk->len - 1; i++) {
		parsed_tk = ec_tk_parse(tk->table[i], str + len);
		if (parsed_tk == NULL)
			break;

		len += strlen(parsed_tk->str);
		ec_parsed_tk_free(parsed_tk);
	}

	completed_tk = ec_tk_complete(tk->table[i], str + len);

	return completed_tk;
}

static void ec_tk_seq_free_priv(struct ec_tk *gen_tk)
{
	struct ec_tk_seq *tk = (struct ec_tk_seq *)gen_tk;
	unsigned int i;

	for (i = 0; i < tk->len; i++)
		ec_tk_free(tk->table[i]);
	ec_free(tk->table);
}

static struct ec_tk_ops ec_tk_seq_ops = {
	.parse = ec_tk_seq_parse,
	.complete = ec_tk_seq_complete,
	.free_priv = ec_tk_seq_free_priv,
};

struct ec_tk *ec_tk_seq_new(const char *id)
{
	struct ec_tk_seq *tk = NULL;

	tk = (struct ec_tk_seq *)ec_tk_new(id, &ec_tk_seq_ops, sizeof(*tk));
	if (tk == NULL)
		return NULL;

	tk->table = NULL;
	tk->len = 0;

	return &tk->gen;
}

struct ec_tk *ec_tk_seq_new_list(const char *id, ...)
{
	struct ec_tk_seq *tk = NULL;
	struct ec_tk *child;
	va_list ap;

	va_start(ap, id);

	tk = (struct ec_tk_seq *)ec_tk_seq_new(id);
	if (tk == NULL)
		goto fail;

	for (child = va_arg(ap, struct ec_tk *);
	     child != EC_TK_ENDLIST;
	     child = va_arg(ap, struct ec_tk *)) {
		if (child == NULL)
			goto fail;

		ec_tk_seq_add(&tk->gen, child);
	}

	va_end(ap);
	return &tk->gen;

fail:
	ec_tk_free(&tk->gen); /* will also free children */
	va_end(ap);
	return NULL;
}

int ec_tk_seq_add(struct ec_tk *gen_tk, struct ec_tk *child)
{
	struct ec_tk_seq *tk = (struct ec_tk_seq *)gen_tk;
	struct ec_tk **table;

	// XXX check tk type

	assert(tk != NULL);
	assert(child != NULL);

	table = ec_realloc(tk->table, (tk->len + 1) * sizeof(*tk->table));
	if (table == NULL)
		return -1;

	tk->table = table;
	table[tk->len] = child;
	tk->len ++;

	return 0;
}

static int ec_tk_seq_testcase(void)
{
	struct ec_tk *tk;
	int ret = 0;

	/* all inputs starting with foo should match */
	tk = ec_tk_seq_new_list(NULL,
		ec_tk_str_new(NULL, "foo"),
		ec_tk_str_new(NULL, "bar"),
		EC_TK_ENDLIST);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "foobar", "foobar");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "foobarxxx", "foobar");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, " foobar", NULL);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "foo", NULL);
	ret |= EC_TEST_CHECK_TK_PARSE(tk, "bar", NULL);
	ec_tk_free(tk);

	/* test completion */
	tk = ec_tk_seq_new_list(NULL,
		ec_tk_str_new(NULL, "foo"),
		ec_tk_str_new(NULL, "bar"),
		EC_TK_ENDLIST);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "", "foo");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "f", "oo");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "foo", "bar");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "foob", "ar");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "foobar", "");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "x", NULL);
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk, "foobarx", NULL);
	ec_tk_free(tk);

	return ret;
}

static struct ec_test ec_tk_seq_test = {
	.name = "tk_seq",
	.test = ec_tk_seq_testcase,
};

EC_REGISTER_TEST(ec_tk_seq_test);
