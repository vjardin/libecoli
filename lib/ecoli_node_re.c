/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <regex.h>

#include <ecoli_log.h>
#include <ecoli_malloc.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_parsed.h>
#include <ecoli_completed.h>
#include <ecoli_node_re.h>

EC_LOG_TYPE_REGISTER(node_re);

struct ec_node_re {
	struct ec_node gen;
	char *re_str;
	regex_t re;
};

static int
ec_node_re_parse(const struct ec_node *gen_node,
		struct ec_parsed *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_re *node = (struct ec_node_re *)gen_node;
	const char *str;
	regmatch_t pos;

	(void)state;

	if (ec_strvec_len(strvec) == 0)
		return EC_PARSED_NOMATCH;

	str = ec_strvec_val(strvec, 0);
	if (regexec(&node->re, str, 1, &pos, 0) != 0)
		return EC_PARSED_NOMATCH;
	if (pos.rm_so != 0 || pos.rm_eo != (int)strlen(str))
		return EC_PARSED_NOMATCH;

	return 1;
}

static void ec_node_re_free_priv(struct ec_node *gen_node)
{
	struct ec_node_re *node = (struct ec_node_re *)gen_node;

	if (node->re_str != NULL) {
		ec_free(node->re_str);
		regfree(&node->re);
	}
}

static struct ec_node_type ec_node_re_type = {
	.name = "re",
	.parse = ec_node_re_parse,
	.complete = ec_node_complete_unknown,
	.size = sizeof(struct ec_node_re),
	.free_priv = ec_node_re_free_priv,
};

EC_NODE_TYPE_REGISTER(ec_node_re_type);

int ec_node_re_set_regexp(struct ec_node *gen_node, const char *str)
{
	struct ec_node_re *node = (struct ec_node_re *)gen_node;
	char *str_copy = NULL;
	regex_t re;
	int ret;

	if (str == NULL)
		return -EINVAL;

	ret = -ENOMEM;
	str_copy = ec_strdup(str);
	if (str_copy == NULL)
		goto fail;

	ret = -EINVAL;
	if (regcomp(&re, str_copy, REG_EXTENDED) != 0)
		goto fail;

	if (node->re_str != NULL) {
		ec_free(node->re_str);
		regfree(&node->re);
	}
	node->re_str = str_copy;
	node->re = re;

	return 0;

fail:
	ec_free(str_copy);
	return ret;
}

struct ec_node *ec_node_re(const char *id, const char *re_str)
{
	struct ec_node *gen_node = NULL;

	gen_node = __ec_node(&ec_node_re_type, id);
	if (gen_node == NULL)
		goto fail;

	if (ec_node_re_set_regexp(gen_node, re_str) < 0)
		goto fail;

	return gen_node;

fail:
	ec_node_free(gen_node);
	return NULL;
}

/* LCOV_EXCL_START */
static int ec_node_re_testcase(void)
{
	struct ec_node *node;
	int testres = 0;

	node = ec_node_re(EC_NO_ID, "fo+|bar");
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "bar");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "foobar");
	testres |= EC_TEST_CHECK_PARSE(node, -1, " foo");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "");
	ec_node_free(node);

	return testres;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_re_test = {
	.name = "node_re",
	.test = ec_node_re_testcase,
};

EC_TEST_REGISTER(ec_node_re_test);
