/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017, Olivier MATZ <zer0@droids-corp.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <ecoli_log.h>
#include <ecoli_malloc.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_keyval.h>
#include <ecoli_node.h>
#include <ecoli_parse.h>
#include <ecoli_complete.h>
#include <ecoli_string.h>
#include <ecoli_node_str.h>
#include <ecoli_node_many.h>

#include "ecoli_node_dynamic.h"

EC_LOG_TYPE_REGISTER(node_dynamic);

struct ec_node_dynamic {
	struct ec_node gen;
	ec_node_dynamic_build_t build;
	void *opaque;
};

static int
ec_node_dynamic_parse(const struct ec_node *gen_node,
		struct ec_parse *parse,
		const struct ec_strvec *strvec)
{
	struct ec_node_dynamic *node = (struct ec_node_dynamic *)gen_node;
	struct ec_node *child = NULL;
	void (*node_free)(struct ec_node *) = ec_node_free;
	char key[64];
	int ret = -1;

	child = node->build(parse, node->opaque);
	if (child == NULL)
		goto fail;

	/* add the node pointer in the attributes, so it will be freed
	 * when parse is freed */
	snprintf(key, sizeof(key), "_dyn_%p", child);
	ret = ec_keyval_set(ec_parse_get_attrs(parse), key, child,
			(void *)node_free);
	if (ret < 0) {
		child = NULL; /* already freed */
		goto fail;
	}

	return ec_node_parse_child(child, parse, strvec);

fail:
	ec_node_free(child);
	return ret;
}

static int
ec_node_dynamic_complete(const struct ec_node *gen_node,
		struct ec_comp *comp,
		const struct ec_strvec *strvec)
{
	struct ec_node_dynamic *node = (struct ec_node_dynamic *)gen_node;
	struct ec_parse *parse;
	struct ec_node *child = NULL;
	void (*node_free)(struct ec_node *) = ec_node_free;
	char key[64];
	int ret = -1;

	parse = ec_comp_get_state(comp);
	child = node->build(parse, node->opaque);
	if (child == NULL)
		goto fail;

	/* add the node pointer in the attributes, so it will be freed
	 * when parse is freed */
	snprintf(key, sizeof(key), "_dyn_%p", child);
	ret = ec_keyval_set(comp->attrs, key, child,
			(void *)node_free);
	if (ret < 0) {
		child = NULL; /* already freed */
		goto fail;
	}

	return ec_node_complete_child(child, comp, strvec);

fail:
	ec_node_free(child);
	return ret;
}

static struct ec_node_type ec_node_dynamic_type = {
	.name = "dynamic",
	.parse = ec_node_dynamic_parse,
	.complete = ec_node_dynamic_complete,
	.size = sizeof(struct ec_node_dynamic),
};

struct ec_node *
ec_node_dynamic(const char *id, ec_node_dynamic_build_t build, void *opaque)
{
	struct ec_node *gen_node = NULL;
	struct ec_node_dynamic *node;

	if (build == NULL) {
		errno = EINVAL;
		goto fail;
	}

	gen_node = __ec_node(&ec_node_dynamic_type, id);
	if (gen_node == NULL)
		goto fail;

	node = (struct ec_node_dynamic *)gen_node;
	node->build = build;
	node->opaque = opaque;

	return gen_node;

fail:
	ec_node_free(gen_node);
	return NULL;

}

EC_NODE_TYPE_REGISTER(ec_node_dynamic_type);

static struct ec_node *
build_counter(struct ec_parse *parse, void *opaque)
{
	const struct ec_node *node;
	struct ec_parse *iter;
	unsigned int count = 0;
	char buf[32];

	(void)opaque;
	for (iter = ec_parse_get_root(parse); iter != NULL;
	     iter = ec_parse_iter_next(iter)) {
		node = ec_parse_get_node(iter);
		if (node->id && !strcmp(node->id, "my-id"))
			count++;
	}
	snprintf(buf, sizeof(buf), "count-%u", count);

	return ec_node_str("my-id", buf);
}

/* LCOV_EXCL_START */
static int ec_node_dynamic_testcase(void)
{
	struct ec_node *node;
	int testres = 0;

	node = ec_node_many(EC_NO_ID,
			ec_node_dynamic(EC_NO_ID, build_counter, NULL),
			1, 3);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 1, "count-0");
	testres |= EC_TEST_CHECK_PARSE(node, 3, "count-0", "count-1", "count-2");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "count-0", "count-0");

	/* test completion */

	testres |= EC_TEST_CHECK_COMPLETE(node,
		"c", EC_NODE_ENDLIST,
		"count-0", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"count-0", "", EC_NODE_ENDLIST,
		"count-1", EC_NODE_ENDLIST,
		"count-1");
	ec_node_free(node);

	return testres;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_dynamic_test = {
	.name = "node_dynamic",
	.test = ec_node_dynamic_testcase,
};

EC_TEST_REGISTER(ec_node_dynamic_test);
