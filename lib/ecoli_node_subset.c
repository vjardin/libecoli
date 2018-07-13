/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>
#include <stdbool.h>

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_parse.h>
#include <ecoli_complete.h>
#include <ecoli_node_subset.h>
#include <ecoli_node_str.h>
#include <ecoli_node_or.h>
#include <ecoli_test.h>

EC_LOG_TYPE_REGISTER(node_subset);

struct ec_node_subset {
	struct ec_node gen;
	struct ec_node **table;
	unsigned int len;
};

struct parse_result {
	size_t parse_len;           /* number of parsed nodes */
	size_t len;                 /* consumed strings */
};

/* recursively find the longest list of nodes that matches: the state is
 * updated accordingly. */
static int
__ec_node_subset_parse(struct parse_result *out, struct ec_node **table,
		size_t table_len, struct ec_parse *state,
		const struct ec_strvec *strvec)
{
	struct ec_node **child_table;
	struct ec_strvec *childvec = NULL;
	size_t i, j, len = 0;
	struct parse_result best_result, result;
	struct ec_parse *best_parse = NULL;
	int ret;

	if (table_len == 0)
		return 0;

	memset(&best_result, 0, sizeof(best_result));

	child_table = ec_calloc(table_len - 1, sizeof(*child_table));
	if (child_table == NULL)
		goto fail;

	for (i = 0; i < table_len; i++) {
		/* try to parse elt i */
		ret = ec_node_parse_child(table[i], state, strvec);
		if (ret < 0)
			goto fail;

		if (ret == EC_PARSE_NOMATCH)
			continue;

		/* build a new table without elt i */
		for (j = 0; j < table_len; j++) {
			if (j < i)
				child_table[j] = table[j];
			else if (j > i)
				child_table[j - 1] = table[j];
		}

		/* build a new strvec (ret is the len of matched strvec) */
		len = ret;
		childvec = ec_strvec_ndup(strvec, len,
					ec_strvec_len(strvec) - len);
		if (childvec == NULL)
			goto fail;

		memset(&result, 0, sizeof(result));
		ret = __ec_node_subset_parse(&result, child_table,
					table_len - 1, state, childvec);
		ec_strvec_free(childvec);
		childvec = NULL;
		if (ret < 0)
			goto fail;

		/* if result is not the best, ignore */
		if (result.parse_len < best_result.parse_len) {
			memset(&result, 0, sizeof(result));
			ec_parse_del_last_child(state);
			continue;
		}

		/* replace the previous best result */
		ec_parse_free(best_parse);
		best_parse = ec_parse_get_last_child(state);
		ec_parse_unlink_child(state, best_parse);

		best_result.parse_len = result.parse_len + 1;
		best_result.len = len + result.len;

		memset(&result, 0, sizeof(result));
	}

	*out = best_result;
	ec_free(child_table);
	if (best_parse != NULL)
		ec_parse_link_child(state, best_parse);

	return 0;

 fail:
	ec_parse_free(best_parse);
	ec_strvec_free(childvec);
	ec_free(child_table);
	return -1;
}

static int
ec_node_subset_parse(const struct ec_node *gen_node,
		struct ec_parse *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_subset *node = (struct ec_node_subset *)gen_node;
	struct ec_parse *parse = NULL;
	struct parse_result result;
	int ret;

	memset(&result, 0, sizeof(result));

	ret = __ec_node_subset_parse(&result, node->table,
				node->len, state, strvec);
	if (ret < 0)
		goto fail;

	/* if no child node matches, return a matching empty strvec */
	if (result.parse_len == 0)
		return 0;

	return result.len;

 fail:
	ec_parse_free(parse);
	return ret;
}

static int
__ec_node_subset_complete(struct ec_node **table, size_t table_len,
			struct ec_comp *comp,
			const struct ec_strvec *strvec)
{
	struct ec_parse *parse = ec_comp_get_state(comp);
	struct ec_strvec *childvec = NULL;
	struct ec_node *save;
	size_t i, len;
	int ret;

	/*
	 * example with table = [a, b, c]
	 * subset_complete([a,b,c], strvec) returns:
	 *   complete(a, strvec) + complete(b, strvec) + complete(c, strvec) +
	 *   + __subset_complete([b, c], childvec) if a matches
	 *   + __subset_complete([a, c], childvec) if b matches
	 *   + __subset_complete([a, b], childvec) if c matches
	 */

	/* first, try to complete with each node of the table */
	for (i = 0; i < table_len; i++) {
		if (table[i] == NULL)
			continue;

		ret = ec_node_complete_child(table[i],
					comp, strvec);
		if (ret < 0)
			goto fail;
	}

	/* then, if a node matches, advance in strvec and try to complete with
	 * all the other nodes */
	for (i = 0; i < table_len; i++) {
		if (table[i] == NULL)
			continue;

		ret = ec_node_parse_child(table[i], parse, strvec);
		if (ret < 0)
			goto fail;

		if (ret == EC_PARSE_NOMATCH)
			continue;

		len = ret;
		childvec = ec_strvec_ndup(strvec, len,
					ec_strvec_len(strvec) - len);
		if (childvec == NULL) {
			ec_parse_del_last_child(parse);
			goto fail;
		}

		save = table[i];
		table[i] = NULL;
		ret = __ec_node_subset_complete(table, table_len,
						comp, childvec);
		table[i] = save;
		ec_strvec_free(childvec);
		childvec = NULL;
		ec_parse_del_last_child(parse);

		if (ret < 0)
			goto fail;
	}

	return 0;

fail:
	return -1;
}

static int
ec_node_subset_complete(const struct ec_node *gen_node,
			struct ec_comp *comp,
			const struct ec_strvec *strvec)
{
	struct ec_node_subset *node = (struct ec_node_subset *)gen_node;

	return __ec_node_subset_complete(node->table, node->len, comp,
					strvec);
}

static void ec_node_subset_free_priv(struct ec_node *gen_node)
{
	struct ec_node_subset *node = (struct ec_node_subset *)gen_node;

	ec_free(node->table);
}

static size_t
ec_node_subset_get_children_count(const struct ec_node *gen_node)
{
	struct ec_node_subset *node = (struct ec_node_subset *)gen_node;
	return node->len;
}

static struct ec_node *
ec_node_subset_get_child(const struct ec_node *gen_node, size_t i)
{
	struct ec_node_subset *node = (struct ec_node_subset *)gen_node;

	if (i >= node->len)
		return NULL;

	return node->table[i];
}

static struct ec_node_type ec_node_subset_type = {
	.name = "subset",
	.parse = ec_node_subset_parse,
	.complete = ec_node_subset_complete,
	.size = sizeof(struct ec_node_subset),
	.free_priv = ec_node_subset_free_priv,
	.get_children_count = ec_node_subset_get_children_count,
	.get_child = ec_node_subset_get_child,
};

EC_NODE_TYPE_REGISTER(ec_node_subset_type);

int ec_node_subset_add(struct ec_node *gen_node, struct ec_node *child)
{
	struct ec_node_subset *node = (struct ec_node_subset *)gen_node;
	struct ec_node **table;

	assert(node != NULL); // XXX specific assert for it, like in libyang

	if (child == NULL) {
		errno = EINVAL;
		goto fail;
	}

	if (ec_node_check_type(gen_node, &ec_node_subset_type) < 0)
		goto fail;

	table = ec_realloc(node->table, (node->len + 1) * sizeof(*node->table));
	if (table == NULL) {
		ec_node_free(child);
		return -1;
	}

	node->table = table;
	table[node->len] = child;
	node->len++;

	return 0;

fail:
	ec_node_free(child);
	return -1;
}

struct ec_node *__ec_node_subset(const char *id, ...)
{
	struct ec_node *gen_node = NULL;
	struct ec_node_subset *node = NULL;
	struct ec_node *child;
	va_list ap;
	int fail = 0;

	va_start(ap, id);

	gen_node = __ec_node(&ec_node_subset_type, id);
	node = (struct ec_node_subset *)gen_node;
	if (node == NULL)
		fail = 1;;

	for (child = va_arg(ap, struct ec_node *);
	     child != EC_NODE_ENDLIST;
	     child = va_arg(ap, struct ec_node *)) {

		/* on error, don't quit the loop to avoid leaks */
		if (fail == 1 || child == NULL ||
				ec_node_subset_add(gen_node, child) < 0) {
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
static int ec_node_subset_testcase(void)
{
	struct ec_node *node;
	int testres = 0;

	node = EC_NODE_SUBSET(EC_NO_ID,
		EC_NODE_OR(EC_NO_ID,
			ec_node_str(EC_NO_ID, "foo"),
			ec_node_str(EC_NO_ID, "bar")),
		ec_node_str(EC_NO_ID, "bar"),
		ec_node_str(EC_NO_ID, "toto")
	);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 0);
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 2, "foo", "bar", "titi");
	testres |= EC_TEST_CHECK_PARSE(node, 3, "bar", "foo", "toto");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo", "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 2, "bar", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 2, "bar", "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 0, " ");
	testres |= EC_TEST_CHECK_PARSE(node, 0, "foox");
	ec_node_free(node);

	/* test completion */
	node = EC_NODE_SUBSET(EC_NO_ID,
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
		"", EC_NODE_ENDLIST,
		"bar2", "bar", "foo", "toto", "titi", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"bar", "bar2", "", EC_NODE_ENDLIST,
		"foo", "toto", "titi", EC_NODE_ENDLIST);
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
		"bar", "b", EC_NODE_ENDLIST,
		"bar2", EC_NODE_ENDLIST);
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

static struct ec_test ec_node_subset_test = {
	.name = "node_subset",
	.test = ec_node_subset_testcase,
};

EC_TEST_REGISTER(ec_node_subset_test);
