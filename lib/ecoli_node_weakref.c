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
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_parse.h>
#include <ecoli_complete.h>
#include <ecoli_node_str.h>
#include <ecoli_node_option.h>
#include <ecoli_node_weakref.h>
#include <ecoli_node_int.h>
#include <ecoli_node_seq.h>
#include <ecoli_node_or.h>

EC_LOG_TYPE_REGISTER(node_weakref);

struct ec_node_weakref {
	struct ec_node gen;
	struct ec_node *child;
};

static int
ec_node_weakref_parse(const struct ec_node *gen_node,
		struct ec_parse *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_weakref *node = (struct ec_node_weakref *)gen_node;

	return ec_node_parse_child(node->child, state, strvec);
}

static int
ec_node_weakref_complete(const struct ec_node *gen_node,
			struct ec_comp *comp,
			const struct ec_strvec *strvec)
{
	struct ec_node_weakref *node = (struct ec_node_weakref *)gen_node;

	return ec_node_complete_child(node->child, comp, strvec);
}

static struct ec_node_type ec_node_weakref_type = {
	.name = "weakref",
	.parse = ec_node_weakref_parse,
	.complete = ec_node_weakref_complete,
	.size = sizeof(struct ec_node_weakref),
};

EC_NODE_TYPE_REGISTER(ec_node_weakref_type);

int ec_node_weakref_set(struct ec_node *gen_node, struct ec_node *child)
{
	struct ec_node_weakref *node = (struct ec_node_weakref *)gen_node;

	assert(node != NULL);

	if (child == NULL) {
		errno = EINVAL;
		goto fail;
	}

	if (ec_node_check_type(gen_node, &ec_node_weakref_type) < 0)
		goto fail;

	node->child = child;

	return 0;

fail:
	/* do not free child */
	return -1;
}

struct ec_node *ec_node_weakref(const char *id, struct ec_node *child)
{
	struct ec_node *gen_node = NULL;

	if (child == NULL)
		return NULL;

	gen_node = __ec_node(&ec_node_weakref_type, id);
	if (gen_node == NULL)
		return NULL;

	ec_node_weakref_set(gen_node, child);

	return gen_node;
}

/* LCOV_EXCL_START */
static int ec_node_weakref_testcase(void)
{
	struct ec_node *weak = NULL, *expr = NULL, *val = NULL;
	struct ec_node *seq = NULL, *op = NULL;
	int testres = 0;

	expr = ec_node("or", EC_NO_ID);
	val = ec_node_int(EC_NO_ID, 0, 10, 10);
	op = ec_node_str(EC_NO_ID, "!");
	weak = ec_node_weakref(EC_NO_ID, expr);
	if (weak == NULL || expr == NULL || val == NULL || op == NULL)
		goto fail;
	seq = EC_NODE_SEQ(EC_NO_ID, op, weak);
	op = NULL;
	weak = NULL;

	if (ec_node_or_add(expr, seq) < 0)
		goto fail;
	seq = NULL;
	if (ec_node_or_add(expr, val) < 0)
		goto fail;
	val = NULL;

	testres |= EC_TEST_CHECK_PARSE(expr, 1, "1");
	testres |= EC_TEST_CHECK_PARSE(expr, 2, "!", "1");
	testres |= EC_TEST_CHECK_PARSE(expr, 3, "!", "!", "1");

	testres |= EC_TEST_CHECK_COMPLETE(expr,
		"", EC_NODE_ENDLIST,
		"!", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(expr,
		"!", "", EC_NODE_ENDLIST,
		"!", EC_NODE_ENDLIST);

	ec_node_free(expr);

	return testres;

fail:
	ec_node_free(weak);
	ec_node_free(expr);
	ec_node_free(val);
	ec_node_free(seq);
	ec_node_free(op);
	return -1;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_weakref_test = {
	.name = "node_weakref",
	.test = ec_node_weakref_testcase,
};

EC_TEST_REGISTER(ec_node_weakref_test);
