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
#include <ctype.h>
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_tk.h>
#include <ecoli_tk_seq.h>
#include <ecoli_tk_str.h>
#include <ecoli_tk_option.h>
#include <ecoli_tk_sh_lex.h>

struct ec_tk_sh_lex {
	struct ec_tk gen;
	struct ec_tk *child;
};

static size_t eat_spaces(const char *str)
{
	size_t i = 0;

	/* skip spaces */
	while (isblank(str[i]))
		i++;

	return i;
}

/*
 * Allocate a new string which is a copy of the input string with quotes
 * removed. If quotes are not closed properly, set missing_quote to the
 * missing quote char.
 */
static char *unquote_str(const char *str, size_t n, int allow_missing_quote,
	char *missing_quote)
{
	unsigned s = 1, d = 0;
	char quote = str[0];
	char *dst;
	int closed = 0;

	dst = ec_malloc(n);
	if (dst == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	/* copy token and remove quotes */
	while (s < n && d < n && str[s] != '\0') {
		if (str[s] == '\\' && str[s+1] == quote) {
			dst[d++] = quote;
			s += 2;
			continue;
		}
		if (str[s] == '\\' && str[s+1] == '\\') {
			dst[d++] = '\\';
			s += 2;
			continue;
		}
		if (str[s] == quote) {
			s++;
			closed = 1;
			break;
		}
		dst[d++] = str[s++];
	}

	/* not enough room in dst buffer (should not happen) */
	if (d >= n) {
		ec_free(dst);
		errno = EMSGSIZE;
		return NULL;
	}

	/* quote not closed */
	if (closed == 0) {
		if (missing_quote != NULL)
			*missing_quote = str[0];
		if (allow_missing_quote == 0) {
			ec_free(dst);
			errno = EINVAL;
			return NULL;
		}
	}
	dst[d++] = '\0';

	return dst;
}

static size_t eat_quoted_str(const char *str)
{
	size_t i = 0;
	char quote = str[0];

	while (str[i] != '\0') {
		if (str[i] != '\\' && str[i+1] == quote)
			return i + 2;
		i++;
	}

	/* unclosed quote, will be detected later */
	return i;
}

static size_t eat_str(const char *str)
{
	size_t i = 0;

	/* skip spaces */
	while (!isblank(str[i]) && str[i] != '\0')
		i++;

	return i;
}

static struct ec_strvec *tokenize(const char *str, int completion,
	int allow_missing_quote, char *missing_quote)
{
	struct ec_strvec *strvec = NULL;
	size_t off = 0, len, suboff, sublen;
	char *word = NULL, *concat = NULL, *tmp;
	int last_is_space = 1;

//	printf("str=%s\n", str);

	strvec = ec_strvec_new();
	if (strvec == NULL)
		goto fail;

	while (str[off] != '\0') {
		len = eat_spaces(&str[off]);
		if (len > 0)
			last_is_space = 1;
//		printf("space=%zd\n", len);
		off += len;

		len = 0;
		suboff = off;
		while (str[suboff] != '\0') {
			last_is_space = 0;
			if (str[suboff] == '"' || str[suboff] == '\'') {
				sublen = eat_quoted_str(&str[suboff]);
//				printf("sublen=%zd\n", sublen);
				word = unquote_str(&str[suboff], sublen,
					allow_missing_quote, missing_quote);
			} else {
				sublen = eat_str(&str[suboff]);
//				printf("sublen=%zd\n", sublen);
				if (sublen == 0)
					break;
				word = ec_strndup(&str[suboff], sublen);
			}

			if (word == NULL)
				goto fail;
//			printf("word=%s\n", word);

			len += sublen;
			suboff += sublen;

			if (concat == NULL) {
				concat = word;
				word = NULL;
			} else {
				tmp = ec_realloc(concat, len + 1);
				if (tmp == NULL)
					goto fail;
				concat = tmp;
				strcat(concat, word);
				ec_free(word);
				word = NULL;
			}
		}

		if (concat != NULL) {
			if (ec_strvec_add(strvec, concat) < 0)
				goto fail;
			ec_free(concat);
			concat = NULL;
		}

		/* XXX remove all printf comments */
//		printf("str off=%zd len=%zd\n", off, len);
		off += len;
	}

	/* in completion mode, append an empty token if the string ends
	 * with space */
	if (completion && last_is_space) {
		if (ec_strvec_add(strvec, "") < 0)
			goto fail;
	}

	return strvec;

 fail:
	ec_free(word);
	ec_free(concat);
	ec_strvec_free(strvec);
	return NULL;
}

static struct ec_parsed_tk *ec_tk_sh_lex_parse(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_sh_lex *tk = (struct ec_tk_sh_lex *)gen_tk;
	struct ec_strvec *new_vec = NULL, *match_strvec;
	struct ec_parsed_tk *parsed_tk = NULL, *child_parsed_tk;
	const char *str;

	parsed_tk = ec_parsed_tk_new();
	if (parsed_tk == NULL)
		return NULL;

	if (ec_strvec_len(strvec) == 0)
		return parsed_tk;

	str = ec_strvec_val(strvec, 0);
	new_vec = tokenize(str, 0, 0, NULL);
	if (new_vec == NULL)
		goto fail;

	child_parsed_tk = ec_tk_parse_tokens(tk->child, new_vec);
	if (child_parsed_tk == NULL)
		goto fail;

	if (!ec_parsed_tk_matches(child_parsed_tk) ||
			ec_parsed_tk_len(child_parsed_tk) !=
				ec_strvec_len(new_vec)) {
		ec_strvec_free(new_vec);
		ec_parsed_tk_free(child_parsed_tk);
		return parsed_tk;
	}
	ec_strvec_free(new_vec);
	new_vec = NULL;

	ec_parsed_tk_add_child(parsed_tk, child_parsed_tk);
	match_strvec = ec_strvec_ndup(strvec, 0, 1);
	if (match_strvec == NULL)
		goto fail;
	ec_parsed_tk_set_match(parsed_tk, gen_tk, match_strvec);

	return parsed_tk;

 fail:
	ec_strvec_free(new_vec);
	ec_parsed_tk_free(parsed_tk);

	return NULL;
}

static struct ec_completed_tk *ec_tk_sh_lex_complete(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_sh_lex *tk = (struct ec_tk_sh_lex *)gen_tk;
	struct ec_completed_tk *completed_tk, *child_completed_tk = NULL;
	struct ec_strvec *new_vec = NULL;
	const char *str;
	char missing_quote;

//	printf("==================\n");
	completed_tk = ec_completed_tk_new();
	if (completed_tk == NULL)
		return NULL;

	if (ec_strvec_len(strvec) != 1)
		return completed_tk;

	str = ec_strvec_val(strvec, 0);
	new_vec = tokenize(str, 1, 1, &missing_quote);
	if (new_vec == NULL)
		goto fail;

//	ec_strvec_dump(new_vec, stdout);

	child_completed_tk = ec_tk_complete_tokens(tk->child, new_vec);
	if (child_completed_tk == NULL)
		goto fail;

	ec_strvec_free(new_vec);
	new_vec = NULL;
	ec_completed_tk_merge(completed_tk, child_completed_tk);

	return completed_tk;


#if 0
	for (i = 0, t = &tokens[0]; i < tk->len; i++, t++) {
		if (*(t + 1) != NULL) {
			child_parsed_tk = ec_tk_parse(tk->table[i], *t);
			if (child_parsed_tk == NULL)
				goto fail;

			if (strlen(child_parsed_tk->str) == 0)
				t--;
			else if (strlen(child_parsed_tk->str) != strlen(*t)) {
				ec_parsed_tk_free(child_parsed_tk);
				goto fail;
			}

			ec_parsed_tk_free(child_parsed_tk);
		} else {
			child_completed_tk = ec_tk_complete(tk->table[i], *t);
			if (child_completed_tk == NULL) {
				ec_completed_tk_free(completed_tk);
				return NULL;
			}
			ec_completed_tk_merge(completed_tk, child_completed_tk);

			child_parsed_tk = ec_tk_parse(tk->table[i], "");
			if (child_parsed_tk == NULL)
				break;
			ec_parsed_tk_free(child_parsed_tk);
			t--;
		}
	}

	if (tokens != NULL) {
		for (t = &tokens[0]; *t != NULL; t++)
			ec_free(*t);
		ec_free(tokens);
		tokens = NULL;
	}

	ec_completed_tk_dump(stdout, completed_tk);
#endif

 fail:
	ec_strvec_free(new_vec);
	ec_completed_tk_free(completed_tk);
	return NULL;
}

static void ec_tk_sh_lex_free_priv(struct ec_tk *gen_tk)
{
	struct ec_tk_sh_lex *tk = (struct ec_tk_sh_lex *)gen_tk;

	ec_tk_free(tk->child);
}

static struct ec_tk_type ec_tk_sh_lex_type = {
	.name = "sh_lex",
	.parse = ec_tk_sh_lex_parse,
	.complete = ec_tk_sh_lex_complete,
	.size = sizeof(struct ec_tk_sh_lex),
	.free_priv = ec_tk_sh_lex_free_priv,
};

EC_TK_TYPE_REGISTER(ec_tk_sh_lex_type);

struct ec_tk *ec_tk_sh_lex_new(const char *id, struct ec_tk *child)
{
	struct ec_tk_sh_lex *tk = NULL;

	if (child == NULL)
		return NULL;

	tk = (struct ec_tk_sh_lex *)__ec_tk_new(&ec_tk_sh_lex_type, id);
	if (tk == NULL) {
		ec_tk_free(child);
		return NULL;
	}

	tk->child = child;

	return &tk->gen;
}

static int ec_tk_sh_lex_testcase(void)
{
	struct ec_tk *tk;
	int ret = 0;

	tk = ec_tk_sh_lex_new(NULL,
		EC_TK_SEQ(NULL,
			ec_tk_str(NULL, "foo"),
			ec_tk_option_new(NULL,
				ec_tk_str(NULL, "toto")
			),
			ec_tk_str(NULL, "bar")
		)
	);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "foo bar");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "  foo   bar");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "  'foo' \"bar\"");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "  'f'oo 'toto' bar");
	ec_tk_free(tk);

	/* test completion */
	tk = ec_tk_sh_lex_new(NULL,
		EC_TK_SEQ(NULL,
			ec_tk_str(NULL, "foo"),
			ec_tk_option_new(NULL,
				ec_tk_str(NULL, "toto")
			),
			ec_tk_str(NULL, "bar"),
			ec_tk_str(NULL, "titi")
		)
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
		" ", EC_TK_ENDLIST,
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
		"foo ", EC_TK_ENDLIST,
		"bar", "toto", EC_TK_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"foo t", EC_TK_ENDLIST,
		"oto", EC_TK_ENDLIST,
		"oto");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"foo b", EC_TK_ENDLIST,
		"ar", EC_TK_ENDLIST,
		"ar");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"foo bar", EC_TK_ENDLIST,
		"", EC_TK_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"foo bar ", EC_TK_ENDLIST,
		"titi", EC_TK_ENDLIST,
		"titi");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"foo toto bar ", EC_TK_ENDLIST,
		"titi", EC_TK_ENDLIST,
		"titi");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"x", EC_TK_ENDLIST,
		EC_TK_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_TK_COMPLETE(tk,
		"foo barx", EC_TK_ENDLIST,
		EC_TK_ENDLIST,
		"");

	ec_tk_free(tk);
	return ret;
}

static struct ec_test ec_tk_sh_lex_test = {
	.name = "tk_sh_lex",
	.test = ec_tk_sh_lex_testcase,
};

EC_TEST_REGISTER(ec_tk_sh_lex_test);
