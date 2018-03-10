/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_parsed.h>
#include <ecoli_completed.h>
#include <ecoli_node_str.h>
#include <ecoli_test.h>
#include <ecoli_node_option.h>

EC_LOG_TYPE_REGISTER(node_option);

struct ec_node_option {
	struct ec_node gen;
	struct ec_node *child;
};

static int
ec_node_option_parse(const struct ec_node *gen_node,
		struct ec_parsed *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_option *node = (struct ec_node_option *)gen_node;
	int ret;

	ret = ec_node_parse_child(node->child, state, strvec);
	if (ret < 0)
		return ret;

	if (ret == EC_PARSED_NOMATCH)
		return 0;

	return ret;
}

static int
ec_node_option_complete(const struct ec_node *gen_node,
			struct ec_completed *completed,
			const struct ec_strvec *strvec)
{
	struct ec_node_option *node = (struct ec_node_option *)gen_node;

	return ec_node_complete_child(node->child, completed, strvec);
}

static void ec_node_option_free_priv(struct ec_node *gen_node)
{
	struct ec_node_option *node = (struct ec_node_option *)gen_node;

	ec_node_free(node->child);
}

static struct ec_node_type ec_node_option_type = {
	.name = "option",
	.parse = ec_node_option_parse,
	.complete = ec_node_option_complete,
	.size = sizeof(struct ec_node_option),
	.free_priv = ec_node_option_free_priv,
};

EC_NODE_TYPE_REGISTER(ec_node_option_type);

int ec_node_option_set(struct ec_node *gen_node, struct ec_node *child)
{
	struct ec_node_option *node = (struct ec_node_option *)gen_node;

	if (gen_node == NULL || child == NULL) {
		errno = EINVAL;
		goto fail;
	}

	if (ec_node_check_type(gen_node, &ec_node_option_type) < 0)
		goto fail;

	if (ec_node_add_child(gen_node, child) < 0)
		goto fail;

	node->child = child;

	return 0;

fail:
	ec_node_free(child);
	return -1;
}

struct ec_node *ec_node_option(const char *id, struct ec_node *child)
{
	struct ec_node *gen_node = NULL;

	if (child == NULL)
		goto fail;

	gen_node = __ec_node(&ec_node_option_type, id);
	if (gen_node == NULL)
		goto fail;

	ec_node_option_set(gen_node, child);
	child = NULL;

	return gen_node;

fail:
	ec_node_free(child);
	return NULL;
}

/* LCOV_EXCL_START */
static int ec_node_option_testcase(void)
{
	struct ec_node *node;
	int testres = 0;

	node = ec_node_option(EC_NO_ID, ec_node_str(EC_NO_ID, "foo"));
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 0, "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 0);
	ec_node_free(node);

	/* test completion */
	node = ec_node_option(EC_NO_ID, ec_node_str(EC_NO_ID, "foo"));
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		"foo", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"f", EC_NODE_ENDLIST,
		"foo", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"b", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	ec_node_free(node);

	return testres;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_option_test = {
	.name = "node_option",
	.test = ec_node_option_testcase,
};

EC_TEST_REGISTER(ec_node_option_test);
