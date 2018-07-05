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
#include <ecoli_parse.h>
#include <ecoli_complete.h>
#include <ecoli_node_or.h>
#include <ecoli_node_str.h>
#include <ecoli_test.h>

EC_LOG_TYPE_REGISTER(node_or);

struct ec_node_or {
	struct ec_node gen;
	struct ec_node **table;
	unsigned int len;
};

static int
ec_node_or_parse(const struct ec_node *gen_node,
		struct ec_parse *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_or *node = (struct ec_node_or *)gen_node;
	unsigned int i;
	int ret;

	for (i = 0; i < node->len; i++) {
		ret = ec_node_parse_child(node->table[i], state, strvec);
		if (ret == EC_PARSE_NOMATCH)
			continue;
		return ret;
	}

	return EC_PARSE_NOMATCH;
}

static int
ec_node_or_complete(const struct ec_node *gen_node,
		struct ec_comp *comp,
		const struct ec_strvec *strvec)
{
	struct ec_node_or *node = (struct ec_node_or *)gen_node;
	int ret;
	size_t n;

	for (n = 0; n < node->len; n++) {
		ret = ec_node_complete_child(node->table[n],
					comp, strvec);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void ec_node_or_free_priv(struct ec_node *gen_node)
{
	struct ec_node_or *node = (struct ec_node_or *)gen_node;
	unsigned int i;

	for (i = 0; i < node->len; i++)
		ec_node_free(node->table[i]);
	ec_free(node->table);
}

static size_t
ec_node_or_get_children_count(const struct ec_node *gen_node)
{
	struct ec_node_or *node = (struct ec_node_or *)gen_node;
	return node->len;
}

static struct ec_node *
ec_node_or_get_child(const struct ec_node *gen_node, size_t i)
{
	struct ec_node_or *node = (struct ec_node_or *)gen_node;

	if (i >= node->len)
		return NULL;

	return node->table[i];
}

static struct ec_node_type ec_node_or_type = {
	.name = "or",
	.parse = ec_node_or_parse,
	.complete = ec_node_or_complete,
	.size = sizeof(struct ec_node_or),
	.free_priv = ec_node_or_free_priv,
	.get_children_count = ec_node_or_get_children_count,
	.get_child = ec_node_or_get_child,
};

EC_NODE_TYPE_REGISTER(ec_node_or_type);

int ec_node_or_add(struct ec_node *gen_node, struct ec_node *child)
{
	struct ec_node_or *node = (struct ec_node_or *)gen_node;
	struct ec_node **table;

	assert(node != NULL);

	assert(node != NULL);

	if (child == NULL) {
		errno = EINVAL;
		goto fail;
	}

	if (ec_node_check_type(gen_node, &ec_node_or_type) < 0)
		goto fail;

	table = ec_realloc(node->table, (node->len + 1) * sizeof(*node->table));
	if (table == NULL)
		goto fail;

	node->table = table;
	table[node->len] = child;
	node->len++;

	return 0;

fail:
	ec_node_free(child);
	return -1;
}

struct ec_node *__ec_node_or(const char *id, ...)
{
	struct ec_node *gen_node = NULL;
	struct ec_node_or *node = NULL;
	struct ec_node *child;
	va_list ap;
	int fail = 0;

	va_start(ap, id);

	gen_node = __ec_node(&ec_node_or_type, id);
	node = (struct ec_node_or *)gen_node;
	if (node == NULL)
		fail = 1;;

	for (child = va_arg(ap, struct ec_node *);
	     child != EC_NODE_ENDLIST;
	     child = va_arg(ap, struct ec_node *)) {

		/* on error, don't quit the loop to avoid leaks */
		if (fail == 1 || child == NULL ||
				ec_node_or_add(gen_node, child) < 0) {
			fail = 1;
			ec_node_free(child);
		}
	}

	if (fail == 1)
		goto fail;

	va_end(ap);
	return gen_node;

fail:
	ec_node_free(gen_node); /* will also free children */
	va_end(ap);
	return NULL;
}

/* LCOV_EXCL_START */
static int ec_node_or_testcase(void)
{
	struct ec_node *node;
	int testres = 0;

	node = EC_NODE_OR(EC_NO_ID,
		ec_node_str(EC_NO_ID, "foo"),
		ec_node_str(EC_NO_ID, "bar")
	);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, -1, " ");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "foox");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "toto");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "");
	ec_node_free(node);

	/* test completion */
	node = EC_NODE_OR(EC_NO_ID,
		ec_node_str(EC_NO_ID, "foo"),
		ec_node_str(EC_NO_ID, "bar"),
		ec_node_str(EC_NO_ID, "bar2"),
		ec_node_str(EC_NO_ID, "toto"),
		ec_node_str(EC_NO_ID, "titi")
	);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		"foo", "bar", "bar2", "toto", "titi", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"f", EC_NODE_ENDLIST,
		"foo", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"b", EC_NODE_ENDLIST,
		"bar", "bar2", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"bar", EC_NODE_ENDLIST,
		"bar", "bar2", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"t", EC_NODE_ENDLIST,
		"toto", "titi", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"to", EC_NODE_ENDLIST,
		"toto", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"x", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	ec_node_free(node);

	return testres;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_or_test = {
	.name = "node_or",
	.test = ec_node_or_testcase,
};

EC_TEST_REGISTER(ec_node_or_test);
