/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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
#include <ecoli_node_or.h>
#include <ecoli_node_many.h>
#include <ecoli_node_seq.h>

EC_LOG_TYPE_REGISTER(node_seq);

struct ec_node_seq {
	struct ec_node gen;
	struct ec_node **table;
	size_t len;
};

static int
ec_node_seq_parse(const struct ec_node *gen_node,
		struct ec_parse *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_seq *node = (struct ec_node_seq *)gen_node;
	struct ec_strvec *childvec = NULL;
	size_t len = 0;
	unsigned int i;
	int ret;

	for (i = 0; i < node->len; i++) {
		childvec = ec_strvec_ndup(strvec, len,
			ec_strvec_len(strvec) - len);
		if (childvec == NULL)
			goto fail;

		ret = ec_node_parse_child(node->table[i], state, childvec);
		if (ret < 0)
			goto fail;

		ec_strvec_free(childvec);
		childvec = NULL;

		if (ret == EC_PARSE_NOMATCH) {
			ec_parse_free_children(state);
			return EC_PARSE_NOMATCH;
		}

		len += ret;
	}

	return len;

fail:
	ec_strvec_free(childvec);
	return -1;
}

static int
__ec_node_seq_complete(struct ec_node **table, size_t table_len,
		struct ec_comp *comp,
		const struct ec_strvec *strvec)
{
	struct ec_parse *parse = ec_comp_get_state(comp);
	struct ec_strvec *childvec = NULL;
	unsigned int i;
	int ret;

	if (table_len == 0)
		return 0;

	/*
	 * Example of completion for a sequence node = [n1,n2] and an
	 * input = [a,b,c,d]:
	 *
	 * result = complete(n1, [a,b,c,d]) +
	 *    complete(n2, [b,c,d]) if n1 matches [a] +
	 *    complete(n2, [c,d]) if n1 matches [a,b] +
	 *    complete(n2, [d]) if n1 matches [a,b,c] +
	 *    complete(n2, []) if n1 matches [a,b,c,d]
	 */

	/* first, try to complete with the first node of the table */
	ret = ec_node_complete_child(table[0], comp, strvec);
	if (ret < 0)
		goto fail;

	/* then, if the first node of the table matches the beginning of the
	 * strvec, try to complete the rest */
	for (i = 0; i < ec_strvec_len(strvec); i++) {
		childvec = ec_strvec_ndup(strvec, 0, i);
		if (childvec == NULL)
			goto fail;

		ret = ec_node_parse_child(table[0], parse, childvec);
		if (ret < 0)
			goto fail;

		ec_strvec_free(childvec);
		childvec = NULL;

		if ((unsigned int)ret != i) {
			if (ret != EC_PARSE_NOMATCH)
				ec_parse_del_last_child(parse);
			continue;
		}

		childvec = ec_strvec_ndup(strvec, i, ec_strvec_len(strvec) - i);
		if (childvec == NULL) {
			ec_parse_del_last_child(parse);
			goto fail;
		}

		ret = __ec_node_seq_complete(&table[1],
					table_len - 1,
					comp, childvec);
		ec_parse_del_last_child(parse);
		ec_strvec_free(childvec);
		childvec = NULL;

		if (ret < 0)
			goto fail;
	}

	return 0;

fail:
	ec_strvec_free(childvec);
	return -1;
}

static int
ec_node_seq_complete(const struct ec_node *gen_node,
		struct ec_comp *comp,
		const struct ec_strvec *strvec)
{
	struct ec_node_seq *node = (struct ec_node_seq *)gen_node;

	return __ec_node_seq_complete(node->table, node->len, comp,
				strvec);
}

static void ec_node_seq_free_priv(struct ec_node *gen_node)
{
	struct ec_node_seq *node = (struct ec_node_seq *)gen_node;
	unsigned int i;

	for (i = 0; i < node->len; i++)
		ec_node_free(node->table[i]);
	ec_free(node->table);
}

static size_t
ec_node_seq_get_children_count(const struct ec_node *gen_node)
{
	struct ec_node_seq *node = (struct ec_node_seq *)gen_node;
	return node->len;
}

static struct ec_node *
ec_node_seq_get_child(const struct ec_node *gen_node, size_t i)
{
	struct ec_node_seq *node = (struct ec_node_seq *)gen_node;

	if (i >= node->len)
		return NULL;

	return node->table[i];
}

static struct ec_node_type ec_node_seq_type = {
	.name = "seq",
	.parse = ec_node_seq_parse,
	.complete = ec_node_seq_complete,
	.size = sizeof(struct ec_node_seq),
	.free_priv = ec_node_seq_free_priv,
	.get_children_count = ec_node_seq_get_children_count,
	.get_child = ec_node_seq_get_child,
};

EC_NODE_TYPE_REGISTER(ec_node_seq_type);

int ec_node_seq_add(struct ec_node *gen_node, struct ec_node *child)
{
	struct ec_node_seq *node = (struct ec_node_seq *)gen_node;
	struct ec_node **table;

	assert(node != NULL);

	if (child == NULL) {
		errno = EINVAL;
		goto fail;
	}

	if (ec_node_check_type(gen_node, &ec_node_seq_type) < 0)
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

struct ec_node *__ec_node_seq(const char *id, ...)
{
	struct ec_node *gen_node = NULL;
	struct ec_node_seq *node = NULL;
	struct ec_node *child;
	va_list ap;
	int fail = 0;

	va_start(ap, id);

	gen_node = __ec_node(&ec_node_seq_type, id);
	node = (struct ec_node_seq *)gen_node;
	if (node == NULL)
		fail = 1;;

	for (child = va_arg(ap, struct ec_node *);
	     child != EC_NODE_ENDLIST;
	     child = va_arg(ap, struct ec_node *)) {

		/* on error, don't quit the loop to avoid leaks */
		if (fail == 1 || child == NULL ||
				ec_node_seq_add(&node->gen, child) < 0) {
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
static int ec_node_seq_testcase(void)
{
	struct ec_node *node;
	int testres = 0;

	node = EC_NODE_SEQ(EC_NO_ID,
		ec_node_str(EC_NO_ID, "foo"),
		ec_node_str(EC_NO_ID, "bar")
	);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 2, "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 2, "foo", "bar", "toto");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "foo");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "foox", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "foo", "barx");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "bar", "foo");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "", "foo");
	ec_node_free(node);

	/* test completion */
	node = EC_NODE_SEQ(EC_NO_ID,
		ec_node_str(EC_NO_ID, "foo"),
		ec_node_option(EC_NO_ID, ec_node_str(EC_NO_ID, "toto")),
		ec_node_str(EC_NO_ID, "bar")
	);
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
		"foo", EC_NODE_ENDLIST,
		"foo", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo", "", EC_NODE_ENDLIST,
		"bar", "toto", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo", "t", EC_NODE_ENDLIST,
		"toto", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo", "b", EC_NODE_ENDLIST,
		"bar", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo", "bar", EC_NODE_ENDLIST,
		"bar", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"x", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foobarx", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	ec_node_free(node);

	return testres;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_seq_test = {
	.name = "node_seq",
	.test = ec_node_seq_testcase,
};

EC_TEST_REGISTER(ec_node_seq_test);
