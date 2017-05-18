#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <regex.h>
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_tk.h>
#include <ecoli_tk_many.h>
#include <ecoli_tk_or.h>
#include <ecoli_tk_str.h>
#include <ecoli_tk_int.h>
#include <ecoli_tk_re_lex.h>

struct regexp_pattern {
	char *pattern;
	regex_t r;
	bool keep;
};

struct ec_tk_re_lex {
	struct ec_tk gen;
	struct ec_tk *child;
	struct regexp_pattern *table;
	size_t len;
};

static struct ec_strvec *
tokenize(struct regexp_pattern *table, size_t table_len, const char *str)
{
	struct ec_strvec *strvec = NULL;
	char *dup = NULL;
	char c;
	size_t len, off = 0;
	size_t i;
	int ret;
	regmatch_t pos;

	dup = ec_strdup(str);
	if (dup == NULL)
		goto fail;

	strvec = ec_strvec_new();
	if (strvec == NULL)
		goto fail;

	len = strlen(dup);
	while (off < len) {
		for (i = 0; i < table_len; i++) {
			ret = regexec(&table[i].r, &dup[off], 1, &pos, 0);
			if (ret != 0)
				continue;
			if (pos.rm_so != 0 || pos.rm_eo == 0) {
				ret = -1;
				continue;
			}

			if (table[i].keep == 0)
				break;

			c = dup[pos.rm_eo + off];
			dup[pos.rm_eo + off] = '\0';
			ec_log(EC_LOG_DEBUG, "re_lex match <%s>\n", &dup[off]);
			if (ec_strvec_add(strvec, &dup[off]) < 0)
				goto fail;

			dup[pos.rm_eo + off] = c;
			break;
		}

		if (ret != 0)
			goto fail;

		off += pos.rm_eo;
	}

	ec_free(dup);
	return strvec;

fail:
	ec_free(dup);
	ec_strvec_free(strvec);
	return NULL;
}

static struct ec_parsed_tk *ec_tk_re_lex_parse(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_re_lex *tk = (struct ec_tk_re_lex *)gen_tk;
	struct ec_strvec *new_vec = NULL, *match_strvec;
	struct ec_parsed_tk *parsed_tk = NULL, *child_parsed_tk;
	const char *str;

	parsed_tk = ec_parsed_tk_new();
	if (parsed_tk == NULL)
		return NULL;

	if (ec_strvec_len(strvec) == 0)
		return parsed_tk;

	str = ec_strvec_val(strvec, 0);
	new_vec = tokenize(tk->table, tk->len, str);
	if (new_vec == NULL)
		goto fail;

	printf("--------\n");
	ec_strvec_dump(stdout, new_vec);
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

static void ec_tk_re_lex_free_priv(struct ec_tk *gen_tk)
{
	struct ec_tk_re_lex *tk = (struct ec_tk_re_lex *)gen_tk;
	unsigned int i;

	for (i = 0; i < tk->len; i++) {
		ec_free(tk->table[i].pattern);
		regfree(&tk->table[i].r);
	}

	ec_free(tk->table);
	ec_tk_free(tk->child);
}

static struct ec_tk_type ec_tk_re_lex_type = {
	.name = "re_lex",
	.parse = ec_tk_re_lex_parse,
	//.complete = ec_tk_re_lex_complete, //XXX
	.free_priv = ec_tk_re_lex_free_priv,
};

EC_TK_TYPE_REGISTER(ec_tk_re_lex_type);

int ec_tk_re_lex_add(struct ec_tk *gen_tk, const char *pattern, int keep)
{
	struct ec_tk_re_lex *tk = (struct ec_tk_re_lex *)gen_tk;
	struct regexp_pattern *table;
	int ret;
	char *pat_dup = NULL;

	ret = -ENOMEM;
	pat_dup = ec_strdup(pattern);
	if (pat_dup == NULL)
		goto fail;

	ret = -ENOMEM;
	table = ec_realloc(tk->table, sizeof(*table) * (tk->len + 1));
	if (table == NULL)
		goto fail;

	ret = regcomp(&table[tk->len].r, pattern, REG_EXTENDED);
	if (ret != 0) {
		ec_log(EC_LOG_ERR,
			"Regular expression <%s> compilation failed: %d\n",
			pattern, ret);
		if (ret == REG_ESPACE)
			ret = -ENOMEM;
		else
			ret = -EINVAL;

		goto fail;
	}

	table[tk->len].pattern = pat_dup;
	table[tk->len].keep = keep;
	tk->len++;
	tk->table = table;

	return 0;

fail:
	ec_free(pat_dup);
	return ret;
}

struct ec_tk *ec_tk_re_lex(const char *id, struct ec_tk *child)
{
	struct ec_tk_re_lex *tk = NULL;

	if (child == NULL)
		return NULL;

	tk = (struct ec_tk_re_lex *)ec_tk_new(id, &ec_tk_re_lex_type,
		sizeof(*tk));
	if (tk == NULL) {
		ec_tk_free(child);
		return NULL;
	}

	tk->child = child;

	return &tk->gen;
}


static int ec_tk_re_lex_testcase(void)
{
	struct ec_tk *tk;
	int ret = 0;

	tk = ec_tk_re_lex(NULL,
		ec_tk_many(NULL,
			EC_TK_OR(NULL,
				ec_tk_str(NULL, "foo"),
				ec_tk_str(NULL, "bar"),
				ec_tk_int(NULL, 0, 1000, 0)
			), 0, 0
		)
	);
	if (tk == NULL) {
		ec_log(EC_LOG_ERR, "cannot create tk\n");
		return -1;
	}

	/* XXX add ^ automatically ? */
	ret |= ec_tk_re_lex_add(tk, "[a-zA-Z]+", 1);
	ret |= ec_tk_re_lex_add(tk, "[0-9]+", 1);
	ret |= ec_tk_re_lex_add(tk, "=", 1);
	ret |= ec_tk_re_lex_add(tk, "-", 1);
	ret |= ec_tk_re_lex_add(tk, "\\+", 1);
	ret |= ec_tk_re_lex_add(tk, "[ 	]+", 0);
	if (ret != 0) {
		ec_log(EC_LOG_ERR, "cannot add regexp to token\n");
		ec_tk_free(tk);
		return -1;
	}

	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "  foo bar  324 bar234");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "foo bar324");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, 1, "");
	ret |= EC_TEST_CHECK_TK_PARSE(tk, -1, "foobar");

	ec_tk_free(tk);

	return ret;
}

static struct ec_test ec_tk_re_lex_test = {
	.name = "tk_re_lex",
	.test = ec_tk_re_lex_testcase,
};

EC_TEST_REGISTER(ec_tk_re_lex_test);
