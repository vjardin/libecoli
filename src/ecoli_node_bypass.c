/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019, Olivier MATZ <zer0@droids-corp.org>
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
#include <ecoli_parse.h>
#include <ecoli_complete.h>
#include <ecoli_node_str.h>
#include <ecoli_test.h>
#include <ecoli_config.h>
#include <ecoli_node_bypass.h>

EC_LOG_TYPE_REGISTER(node_bypass);

struct ec_node_bypass {
	struct ec_node gen;
	struct ec_node *child;
};

static int
ec_node_bypass_parse(const struct ec_node *gen_node,
		struct ec_parse *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_bypass *node = (struct ec_node_bypass *)gen_node;

	return ec_node_parse_child(node->child, state, strvec);
}

static int
ec_node_bypass_complete(const struct ec_node *gen_node,
			struct ec_comp *comp,
			const struct ec_strvec *strvec)
{
	struct ec_node_bypass *node = (struct ec_node_bypass *)gen_node;

	return ec_node_complete_child(node->child, comp, strvec);
}

static void ec_node_bypass_free_priv(struct ec_node *gen_node)
{
	struct ec_node_bypass *node = (struct ec_node_bypass *)gen_node;

	ec_node_free(node->child);
}

static size_t
ec_node_bypass_get_children_count(const struct ec_node *gen_node)
{
	struct ec_node_bypass *node = (struct ec_node_bypass *)gen_node;

	if (node->child)
		return 1;
	return 0;
}

static int
ec_node_bypass_get_child(const struct ec_node *gen_node, size_t i,
	struct ec_node **child, unsigned int *refs)
{
	struct ec_node_bypass *node = (struct ec_node_bypass *)gen_node;

	if (i >= 1)
		return -1;

	*child = node->child;
	*refs = 2;
	return 0;
}

static const struct ec_config_schema ec_node_bypass_schema[] = {
	{
		.key = "child",
		.desc = "The child node.",
		.type = EC_CONFIG_TYPE_NODE,
	},
	{
		.type = EC_CONFIG_TYPE_NONE,
	},
};

static int ec_node_bypass_set_config(struct ec_node *gen_node,
				const struct ec_config *config)
{
	struct ec_node_bypass *node = (struct ec_node_bypass *)gen_node;
	const struct ec_config *child;

	child = ec_config_dict_get(config, "child");
	if (child == NULL)
		goto fail;
	if (ec_config_get_type(child) != EC_CONFIG_TYPE_NODE) {
		errno = EINVAL;
		goto fail;
	}

	if (node->child != NULL)
		ec_node_free(node->child);
	node->child = ec_node_clone(child->node);

	return 0;

fail:
	return -1;
}

static struct ec_node_type ec_node_bypass_type = {
	.name = "bypass",
	.schema = ec_node_bypass_schema,
	.set_config = ec_node_bypass_set_config,
	.parse = ec_node_bypass_parse,
	.complete = ec_node_bypass_complete,
	.size = sizeof(struct ec_node_bypass),
	.free_priv = ec_node_bypass_free_priv,
	.get_children_count = ec_node_bypass_get_children_count,
	.get_child = ec_node_bypass_get_child,
};

EC_NODE_TYPE_REGISTER(ec_node_bypass_type);

int
ec_node_bypass_set_child(struct ec_node *gen_node, struct ec_node *child)
{
	const struct ec_config *cur_config = NULL;
	struct ec_config *config = NULL;
	int ret;

	if (ec_node_check_type(gen_node, &ec_node_bypass_type) < 0)
		goto fail;

	cur_config = ec_node_get_config(gen_node);
	if (cur_config == NULL)
		config = ec_config_dict();
	else
		config = ec_config_dup(cur_config);
	if (config == NULL)
		goto fail;

	if (ec_config_dict_set(config, "child", ec_config_node(child)) < 0) {
		child = NULL; /* freed */
		goto fail;
	}
	child = NULL; /* freed */

	ret = ec_node_set_config(gen_node, config);
	config = NULL; /* freed */
	if (ret < 0)
		goto fail;

	return 0;

fail:
	ec_config_free(config);
	ec_node_free(child);
	return -1;
}

struct ec_node *ec_node_bypass(const char *id, struct ec_node *child)
{
	struct ec_node *gen_node = NULL;

	if (child == NULL)
		goto fail;

	gen_node = ec_node_from_type(&ec_node_bypass_type, id);
	if (gen_node == NULL)
		goto fail;

	ec_node_bypass_set_child(gen_node, child);
	child = NULL;

	return gen_node;

fail:
	ec_node_free(child);
	return NULL;
}

/* LCOV_EXCL_START */
static int ec_node_bypass_testcase(void)
{
	struct ec_node *node;
	int testres = 0;

	node = ec_node_bypass(EC_NO_ID, ec_node_str(EC_NO_ID, "foo"));
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "bar");
	testres |= EC_TEST_CHECK_PARSE(node, -1);
	ec_node_free(node);

	/* test completion */
	node = ec_node_bypass(EC_NO_ID, ec_node_str(EC_NO_ID, "foo"));
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

static struct ec_test ec_node_bypass_test = {
	.name = "node_bypass",
	.test = ec_node_bypass_testcase,
};

EC_TEST_REGISTER(ec_node_bypass_test);
