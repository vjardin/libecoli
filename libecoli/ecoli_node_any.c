/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_parse.h>
#include <ecoli_complete.h>
#include <ecoli_config.h>
#include <ecoli_node_any.h>

EC_LOG_TYPE_REGISTER(node_any);

struct ec_node_any {
	struct ec_node gen;
};

static int ec_node_any_parse(const struct ec_node *gen_node,
			struct ec_parse *state,
			const struct ec_strvec *strvec)
{
	(void)gen_node;
	(void)state;

	if (ec_strvec_len(strvec) == 0)
		return EC_PARSE_NOMATCH;

	return 1;
}

static const struct ec_config_schema ec_node_any_schema[] = {
	{
		.type = EC_CONFIG_TYPE_NONE,
	},
};

static int ec_node_any_set_config(struct ec_node *gen_node,
				const struct ec_config *config)
{
	(void)gen_node;
	(void)config;
	return 0;
}

static struct ec_node_type ec_node_any_type = {
	.name = "any",
	.schema = ec_node_any_schema,
	.set_config = ec_node_any_set_config,
	.parse = ec_node_any_parse,
	.complete = ec_node_complete_unknown,
	.size = sizeof(struct ec_node_any),
};

EC_NODE_TYPE_REGISTER(ec_node_any_type);

/* LCOV_EXCL_START */
static int ec_node_any_testcase(void)
{
	struct ec_node *node;
	int testres = 0;

	node = ec_node("any", EC_NO_ID);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, -1);
	ec_node_free(node);

	/* never completes */
	node = ec_node("any", EC_NO_ID);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	ec_node_free(node);

	return testres;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_any_test = {
	.name = "node_any",
	.test = ec_node_any_testcase,
};

EC_TEST_REGISTER(ec_node_any_test);
