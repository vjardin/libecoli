/*
 * Copyright (c) 2016-2017, Olivier MATZ <zer0@droids-corp.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University of California, Berkeley nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include <ecoli_parsed.h>
#include <ecoli_completed.h>
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
	size_t parsed_len;          /* number of parsed node */
	size_t len;                 /* consumed strings */
};

/* recursively find the longest list of nodes that matches: the state is
 * updated accordingly. */
static int
__ec_node_subset_parse(struct parse_result *out, struct ec_node **table,
		size_t table_len, struct ec_parsed *state,
		const struct ec_strvec *strvec)
{
	struct ec_node **child_table;
	struct ec_strvec *childvec = NULL;
	size_t i, j, len = 0;
	struct parse_result best_result, result;
	struct ec_parsed *best_parsed = NULL;
	int ret;

	if (table_len == 0)
		return 0;

	memset(&best_result, 0, sizeof(best_result));

	child_table = ec_calloc(table_len - 1, sizeof(*child_table));
	if (child_table == NULL) {
		ret = -ENOMEM;
		goto fail;
	}

	for (i = 0; i < table_len; i++) {
		/* try to parse elt i */
		ret = ec_node_parse_child(table[i], state, strvec);
		if (ret < 0)
			goto fail;

		if (ret == EC_PARSED_NOMATCH)
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
		if (childvec == NULL) {
			ret = -ENOMEM;
			goto fail;
		}

		memset(&result, 0, sizeof(result));
		ret = __ec_node_subset_parse(&result, child_table,
					table_len - 1, state, childvec);
		ec_strvec_free(childvec);
		childvec = NULL;
		if (ret < 0)
			goto fail;

		/* if result is not the best, ignore */
		if (result.parsed_len < best_result.parsed_len) {
			memset(&result, 0, sizeof(result));
			ec_parsed_del_last_child(state);
			continue;
		}

		/* replace the previous best result */
		ec_parsed_free(best_parsed);
		best_parsed = ec_parsed_get_last_child(state);
		ec_parsed_del_child(state, best_parsed);

		best_result.parsed_len = result.parsed_len + 1;
		best_result.len = len + result.len;

		memset(&result, 0, sizeof(result));
	}

	*out = best_result;
	ec_free(child_table);
	if (best_parsed != NULL)
		ec_parsed_add_child(state, best_parsed);

	return 0;

 fail:
	ec_parsed_free(best_parsed);
	ec_strvec_free(childvec);
	ec_free(child_table);
	return ret;
}

static int
ec_node_subset_parse(const struct ec_node *gen_node,
		struct ec_parsed *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_subset *node = (struct ec_node_subset *)gen_node;
	struct ec_parsed *parsed = NULL;
	struct parse_result result;
	int ret;

	memset(&result, 0, sizeof(result));

	ret = __ec_node_subset_parse(&result, node->table,
				node->len, state, strvec);
	if (ret < 0)
		goto fail;

	/* if no child node matches, return a matching empty strvec */
	if (result.parsed_len == 0)
		return 0;

	return result.len;

 fail:
	ec_parsed_free(parsed);
	return ret;
}

static int
__ec_node_subset_complete(struct ec_node **table, size_t table_len,
			struct ec_completed *completed,
			struct ec_parsed *parsed,
			const struct ec_strvec *strvec)
{
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
					completed, parsed, strvec);
		if (ret < 0)
			goto fail;
	}

	/* then, if a node matches, advance in strvec and try to complete with
	 * all the other nodes */
	for (i = 0; i < table_len; i++) {
		if (table[i] == NULL)
			continue;

		ret = ec_node_parse_child(table[i], parsed, strvec);
		if (ret < 0)
			goto fail;

		if (ret == EC_PARSED_NOMATCH)
			continue;

		len = ret;
		childvec = ec_strvec_ndup(strvec, len,
					ec_strvec_len(strvec) - len);
		if (childvec == NULL) {
			ec_parsed_del_last_child(parsed);
			goto fail;
		}

		save = table[i];
		table[i] = NULL;
		ret = __ec_node_subset_complete(table, table_len,
						completed, parsed, childvec);
		table[i] = save;
		ec_strvec_free(childvec);
		childvec = NULL;
		ec_parsed_del_last_child(parsed);

		if (ret < 0)
			goto fail;
	}

	return 0;

fail:
	return -1;
}

static int
ec_node_subset_complete(const struct ec_node *gen_node,
			struct ec_completed *completed,
			struct ec_parsed *parsed,
			const struct ec_strvec *strvec)
{
	struct ec_node_subset *node = (struct ec_node_subset *)gen_node;

	return __ec_node_subset_complete(node->table, node->len, completed,
					parsed, strvec);
}

static void ec_node_subset_free_priv(struct ec_node *gen_node)
{
	struct ec_node_subset *node = (struct ec_node_subset *)gen_node;
	unsigned int i;

	for (i = 0; i < node->len; i++)
		ec_node_free(node->table[i]);
	ec_free(node->table);
}

int ec_node_subset_add(struct ec_node *gen_node, struct ec_node *child)
{
	struct ec_node_subset *node = (struct ec_node_subset *)gen_node;
	struct ec_node **table;

	assert(node != NULL);

	if (child == NULL)
		return -EINVAL;

	gen_node->flags &= ~EC_NODE_F_BUILT;

	table = ec_realloc(node->table, (node->len + 1) * sizeof(*node->table));
	if (table == NULL) {
		ec_node_free(child);
		return -1;
	}

	node->table = table;
	table[node->len] = child;
	node->len++;

	child->parent = gen_node;
	TAILQ_INSERT_TAIL(&gen_node->children, child, next);

	return 0;
}

static struct ec_node_type ec_node_subset_type = {
	.name = "subset",
	.parse = ec_node_subset_parse,
	.complete = ec_node_subset_complete,
	.size = sizeof(struct ec_node_subset),
	.free_priv = ec_node_subset_free_priv,
};

EC_NODE_TYPE_REGISTER(ec_node_subset_type);

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
	int ret = 0;

	node = EC_NODE_SUBSET(NULL,
		EC_NODE_OR(NULL,
			ec_node_str(NULL, "foo"),
			ec_node_str(NULL, "bar")),
		ec_node_str(NULL, "bar"),
		ec_node_str(NULL, "toto")
	);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_PARSE(node, 0);
	ret |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 2, "foo", "bar", "titi");
	ret |= EC_TEST_CHECK_PARSE(node, 3, "bar", "foo", "toto");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "foo", "foo");
	ret |= EC_TEST_CHECK_PARSE(node, 2, "bar", "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 2, "bar", "foo");
	ret |= EC_TEST_CHECK_PARSE(node, 0, " ");
	ret |= EC_TEST_CHECK_PARSE(node, 0, "foox");
	ec_node_free(node);

	/* test completion */
	node = EC_NODE_SUBSET(NULL,
		ec_node_str(NULL, "foo"),
		ec_node_str(NULL, "bar"),
		ec_node_str(NULL, "bar2"),
		ec_node_str(NULL, "toto"),
		ec_node_str(NULL, "titi")
	);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		"foo", "bar", "bar2", "toto", "titi", EC_NODE_ENDLIST);
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		"bar2", "bar", "foo", "toto", "titi", EC_NODE_ENDLIST);
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"bar", "bar2", "", EC_NODE_ENDLIST,
		"foo", "toto", "titi", EC_NODE_ENDLIST);
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"f", EC_NODE_ENDLIST,
		"foo", EC_NODE_ENDLIST);
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"b", EC_NODE_ENDLIST,
		"bar", "bar2", EC_NODE_ENDLIST);
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"bar", EC_NODE_ENDLIST,
		"bar", "bar2", EC_NODE_ENDLIST);
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"bar", "b", EC_NODE_ENDLIST,
		"bar2", EC_NODE_ENDLIST);
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"t", EC_NODE_ENDLIST,
		"toto", "titi", EC_NODE_ENDLIST);
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"to", EC_NODE_ENDLIST,
		"toto", EC_NODE_ENDLIST);
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"x", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	ec_node_free(node);

	return ret;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_subset_test = {
	.name = "node_subset",
	.test = ec_node_subset_testcase,
};

EC_TEST_REGISTER(ec_node_subset_test);
