/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
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
#include <ecoli_string.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_parsed.h>
#include <ecoli_complete.h>
#include <ecoli_node_seq.h>
#include <ecoli_node_str.h>
#include <ecoli_node_option.h>
#include <ecoli_node_sh_lex.h>

EC_LOG_TYPE_REGISTER(node_sh_lex);

struct ec_node_sh_lex {
	struct ec_node gen;
	struct ec_node *child;
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

	/* copy string and remove quotes */
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

	/* eat chars until we find a quote, space, or end of string  */
	while (!isblank(str[i]) && str[i] != '\0' &&
			str[i] != '"' && str[i] != '\'')
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

	strvec = ec_strvec();
	if (strvec == NULL)
		goto fail;

	while (str[off] != '\0') {
		if (missing_quote != NULL)
			*missing_quote = '\0';
		len = eat_spaces(&str[off]);
		if (len > 0)
			last_is_space = 1;
		off += len;

		len = 0;
		suboff = off;
		while (str[suboff] != '\0') {
			if (missing_quote != NULL)
				*missing_quote = '\0';
			last_is_space = 0;
			if (str[suboff] == '"' || str[suboff] == '\'') {
				sublen = eat_quoted_str(&str[suboff]);
				word = unquote_str(&str[suboff], sublen,
					allow_missing_quote, missing_quote);
			} else {
				sublen = eat_str(&str[suboff]);
				if (sublen == 0)
					break;
				word = ec_strndup(&str[suboff], sublen);
			}

			if (word == NULL)
				goto fail;

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

		off += len;
	}

	/* in completion mode, append an empty string in the vector if
	 * the input string ends with space */
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

static int
ec_node_sh_lex_parse(const struct ec_node *gen_node,
		struct ec_parsed *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_sh_lex *node = (struct ec_node_sh_lex *)gen_node;
	struct ec_strvec *new_vec = NULL;
	struct ec_parsed *child_parsed;
	const char *str;
	int ret;

	if (ec_strvec_len(strvec) == 0) {
		new_vec = ec_strvec();
	} else {
		str = ec_strvec_val(strvec, 0);
		new_vec = tokenize(str, 0, 0, NULL);
	}
	if (new_vec == NULL) {
		if (errno == EINVAL)
			ret = EC_PARSED_NOMATCH;
		else
			ret = -ENOMEM;
		goto fail;
	}

	ret = ec_node_parse_child(node->child, state, new_vec);
	if (ret < 0)
		goto fail;

	if ((unsigned)ret == ec_strvec_len(new_vec)) {
		ret = 1;
	} else if (ret != EC_PARSED_NOMATCH) {
		child_parsed = ec_parsed_get_last_child(state);
		ec_parsed_unlink_child(state, child_parsed);
		ec_parsed_free(child_parsed);
		ret = EC_PARSED_NOMATCH;
	}

	ec_strvec_free(new_vec);
	new_vec = NULL;

	return ret;

 fail:
	ec_strvec_free(new_vec);
	return ret;
}

static int
ec_node_sh_lex_complete(const struct ec_node *gen_node,
			struct ec_comp *comp,
			const struct ec_strvec *strvec)
{
	struct ec_node_sh_lex *node = (struct ec_node_sh_lex *)gen_node;
	struct ec_comp *tmp_comp = NULL;
	struct ec_strvec *new_vec = NULL;
	struct ec_comp_iter *iter = NULL;
	struct ec_comp_item *item = NULL;
	char *new_str = NULL;
	const char *str;
	char missing_quote = '\0';
	int ret;

	if (ec_strvec_len(strvec) != 1)
		return 0;

	str = ec_strvec_val(strvec, 0);
	new_vec = tokenize(str, 1, 1, &missing_quote);
	if (new_vec == NULL)
		goto fail;

	/* we will store the completions in a temporary struct, because
	 * we want to update them (ex: add missing quotes) */
	tmp_comp = ec_comp(ec_comp_get_state(comp));
	if (tmp_comp == NULL)
		goto fail;

	ret = ec_node_complete_child(node->child, tmp_comp, new_vec);
	if (ret < 0)
		goto fail;

	/* add missing quote for full completions  */
	if (missing_quote != '\0') {
		iter = ec_comp_iter(tmp_comp, EC_COMP_FULL);
		if (iter == NULL)
			goto fail;
		while ((item = ec_comp_iter_next(iter)) != NULL) {
			str = ec_comp_item_get_str(item);
			if (ec_asprintf(&new_str, "%c%s%c", missing_quote, str,
					missing_quote) < 0) {
				new_str = NULL;
				goto fail;
			}
			if (ec_comp_item_set_str(item, new_str) < 0)
				goto fail;
			ec_free(new_str);
			new_str = NULL;

			str = ec_comp_item_get_completion(item);
			if (ec_asprintf(&new_str, "%s%c", str,
					missing_quote) < 0) {
				new_str = NULL;
				goto fail;
			}
			if (ec_comp_item_set_completion(item, new_str) < 0)
				goto fail;
			ec_free(new_str);
			new_str = NULL;
		}
	}

	ec_comp_iter_free(iter);
	ec_strvec_free(new_vec);

	ec_comp_merge(comp, tmp_comp);

	return 0;

 fail:
	ec_comp_free(tmp_comp);
	ec_comp_iter_free(iter);
	ec_strvec_free(new_vec);
	ec_free(new_str);

	return -1;
}

static void ec_node_sh_lex_free_priv(struct ec_node *gen_node)
{
	struct ec_node_sh_lex *node = (struct ec_node_sh_lex *)gen_node;

	ec_node_free(node->child);
}

static struct ec_node_type ec_node_sh_lex_type = {
	.name = "sh_lex",
	.parse = ec_node_sh_lex_parse,
	.complete = ec_node_sh_lex_complete,
	.size = sizeof(struct ec_node_sh_lex),
	.free_priv = ec_node_sh_lex_free_priv,
};

EC_NODE_TYPE_REGISTER(ec_node_sh_lex_type);

struct ec_node *ec_node_sh_lex(const char *id, struct ec_node *child)
{
	struct ec_node_sh_lex *node = NULL;

	if (child == NULL)
		return NULL;

	node = (struct ec_node_sh_lex *)__ec_node(&ec_node_sh_lex_type, id);
	if (node == NULL) {
		ec_node_free(child);
		return NULL;
	}

	node->child = child;

	return &node->gen;
}

/* LCOV_EXCL_START */
static int ec_node_sh_lex_testcase(void)
{
	struct ec_node *node;
	int testres = 0;

	node = ec_node_sh_lex(EC_NO_ID,
		EC_NODE_SEQ(EC_NO_ID,
			ec_node_str(EC_NO_ID, "foo"),
			ec_node_option(EC_NO_ID,
				ec_node_str(EC_NO_ID, "toto")
			),
			ec_node_str(EC_NO_ID, "bar")
		)
	);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo bar");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "  foo   bar");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "  'foo' \"bar\"");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "  'f'oo 'toto' bar");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "  foo toto bar'");
	ec_node_free(node);

	/* test completion */
	node = ec_node_sh_lex(EC_NO_ID,
		EC_NODE_SEQ(EC_NO_ID,
			ec_node_str(EC_NO_ID, "foo"),
			ec_node_option(EC_NO_ID,
				ec_node_str(EC_NO_ID, "toto")
			),
			ec_node_str(EC_NO_ID, "bar"),
			ec_node_str(EC_NO_ID, "titi")
		)
	);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		"foo", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		" ", EC_NODE_ENDLIST,
		"foo", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"f", EC_NODE_ENDLIST,
		"foo", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo", EC_NODE_ENDLIST,
		"foo", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo ", EC_NODE_ENDLIST,
		"bar", "toto", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo t", EC_NODE_ENDLIST,
		"toto", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo b", EC_NODE_ENDLIST,
		"bar", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo bar", EC_NODE_ENDLIST,
		"bar", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo bar ", EC_NODE_ENDLIST,
		"titi", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo toto bar ", EC_NODE_ENDLIST,
		"titi", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"x", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo barx", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo 'b", EC_NODE_ENDLIST,
		"'bar'", EC_NODE_ENDLIST);

	ec_node_free(node);
	return testres;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_sh_lex_test = {
	.name = "node_sh_lex",
	.test = ec_node_sh_lex_testcase,
};

EC_TEST_REGISTER(ec_node_sh_lex_test);
