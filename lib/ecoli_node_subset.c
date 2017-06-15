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

struct ec_node_subset {
	struct ec_node gen;
	struct ec_node **table;
	unsigned int len;
};

struct parse_result {
	struct ec_parsed **parsed_table; /* list of parsed node */
	size_t parsed_table_len;            /* number of parsed node */
	size_t len;                         /* consumed strings */
};

static int __ec_node_subset_parse(struct ec_node **table, size_t table_len,
				const struct ec_strvec *strvec,
				struct parse_result *out)
{
	struct ec_node **child_table;
	struct ec_parsed *child_parsed = NULL;
	struct ec_strvec *childvec = NULL;
	size_t i, j, len = 0;
	struct parse_result best_result, result;
	int ret;

	if (table_len == 0)
		return 0;

	memset(&best_result, 0, sizeof(best_result));
	best_result.parsed_table =
		ec_calloc(table_len, sizeof(*best_result.parsed_table[0]));
	if (best_result.parsed_table == NULL)
		goto fail;

	child_table = ec_calloc(table_len - 1, sizeof(*child_table));
	if (child_table == NULL)
		goto fail;

	for (i = 0; i < table_len; i++) {
		/* try to parse elt i */
		child_parsed = ec_node_parse_strvec(table[i], strvec);
		if (child_parsed == NULL)
			goto fail;

		if (!ec_parsed_matches(child_parsed)) {
			ec_parsed_free(child_parsed);
			child_parsed = NULL;
			continue;
		}

		/* build a new table without elt i */
		for (j = 0; j < table_len; j++) {
			if (j < i)
				child_table[j] = table[j];
			else if (j > i)
				child_table[j - 1] = table[j];
		}

		/* build a new strvec */
		len = ec_parsed_len(child_parsed);
		childvec = ec_strvec_ndup(strvec, len,
					ec_strvec_len(strvec) - len);
		if (childvec == NULL)
			goto fail;

		memset(&result, 0, sizeof(result));

		ret = __ec_node_subset_parse(child_table, table_len - 1,
					childvec, &result);
		ec_strvec_free(childvec);
		childvec = NULL;
		if (ret < 0)
			goto fail;

		/* if result is not the best, ignore */
		if (result.parsed_table_len + 1 <=
				best_result.parsed_table_len) {
			ec_parsed_free(child_parsed);
			child_parsed = NULL;
			for (j = 0; j < result.parsed_table_len; j++)
				ec_parsed_free(result.parsed_table[j]);
			ec_free(result.parsed_table);
			memset(&result, 0, sizeof(result));
			continue;
		}

		/* replace the previous best result */
		for (j = 0; j < best_result.parsed_table_len; j++)
			ec_parsed_free(best_result.parsed_table[j]);
		best_result.parsed_table[0] = child_parsed;
		child_parsed = NULL;
		for (j = 0; j < result.parsed_table_len; j++)
			best_result.parsed_table[j+1] = result.parsed_table[j];
		best_result.parsed_table_len = result.parsed_table_len + 1;
		best_result.len = len + result.len;
		ec_free(result.parsed_table);
	}

	*out = best_result;
	ec_free(child_table);

	return 0;

 fail:
	ec_parsed_free(child_parsed);
	ec_strvec_free(childvec);
	for (j = 0; j < best_result.parsed_table_len; j++)
		ec_parsed_free(best_result.parsed_table[j]);
	ec_free(best_result.parsed_table);
	ec_free(child_table);
	return -1;
}

static struct ec_parsed *ec_node_subset_parse(const struct ec_node *gen_node,
	const struct ec_strvec *strvec)
{
	struct ec_node_subset *node = (struct ec_node_subset *)gen_node;
	struct ec_parsed *parsed = NULL;
	struct parse_result result;
	struct ec_strvec *match_strvec;
	size_t i;
	int ret;

	memset(&result, 0, sizeof(result));

	parsed = ec_parsed();
	if (parsed == NULL)
		goto fail;

	ret = __ec_node_subset_parse(node->table, node->len, strvec, &result);
	if (ret < 0)
		goto fail;

	/* if no child node matches, return a matching empty strvec */
	if (result.parsed_table_len == 0) {
		ec_free(result.parsed_table);
		match_strvec = ec_strvec();
		if (match_strvec == NULL)
			goto fail;
		ec_parsed_set_match(parsed, gen_node, match_strvec);
		return parsed;
	}

	for (i = 0; i < result.parsed_table_len; i++) {
		ec_parsed_add_child(parsed, result.parsed_table[i]);
		result.parsed_table[i] = NULL;
	}
	ec_free(result.parsed_table);
	result.parsed_table = NULL;

	match_strvec = ec_strvec_ndup(strvec, 0, result.len);
	if (match_strvec == NULL)
		goto fail;

	ec_parsed_set_match(parsed, gen_node, match_strvec);

	return parsed;

 fail:
	for (i = 0; i < result.parsed_table_len; i++)
		ec_parsed_free(result.parsed_table[i]);
	ec_free(result.parsed_table);
	ec_parsed_free(parsed);

	return NULL;
}

static struct ec_completed *
__ec_node_subset_complete(struct ec_node **table, size_t table_len,
	const struct ec_strvec *strvec)
{
	struct ec_completed *completed = NULL;
	struct ec_completed *child_completed = NULL;
	struct ec_strvec *childvec = NULL;
	struct ec_parsed *parsed = NULL;
	struct ec_node *save;
	size_t i, len;

	/*
	 * example with table = [a, b, c]
	 * subset_complete([a,b,c], strvec) returns:
	 *   complete(a, strvec) + complete(b, strvec) + complete(c, strvec) +
	 *   + __subset_complete([b, c], childvec) if a matches
	 *   + __subset_complete([a, c], childvec) if b matches
	 *   + __subset_complete([a, b], childvec) if c matches
	 */

	completed = ec_completed();
	if (completed == NULL)
		goto fail;

	/* first, try to complete with each node of the table */
	for (i = 0; i < table_len; i++) {
		if (table[i] == NULL)
			continue;

		child_completed = ec_node_complete_strvec(table[i],
			strvec);

		if (child_completed == NULL)
			goto fail;

		ec_completed_merge(completed, child_completed);
		child_completed = NULL;
	}

	/* then, if a node matches, advance in strvec and try to complete with
	 * all the other nodes */
	for (i = 0; i < table_len; i++) {
		if (table[i] == NULL)
			continue;

		parsed = ec_node_parse_strvec(table[i], strvec);
		if (parsed == NULL)
			goto fail;

		if (!ec_parsed_matches(parsed)) {
			ec_parsed_free(parsed);
			parsed = NULL;
			continue;
		}

		len = ec_parsed_len(parsed);
		childvec = ec_strvec_ndup(strvec, len,
					ec_strvec_len(strvec) - len);
		if (childvec == NULL)
			goto fail;

		save = table[i];
		table[i] = NULL;
		child_completed = __ec_node_subset_complete(table,
							table_len, childvec);
		table[i] = save;
		ec_strvec_free(childvec);
		childvec = NULL;

		if (child_completed == NULL)
			goto fail;

		ec_completed_merge(completed, child_completed);
		child_completed = NULL;

		ec_parsed_free(parsed);
		parsed = NULL;
	}

	return completed;
fail:
	ec_parsed_free(parsed);
	ec_completed_free(child_completed);
	ec_completed_free(completed);

	return NULL;
}

static struct ec_completed *ec_node_subset_complete(const struct ec_node *gen_node,
	const struct ec_strvec *strvec)
{
	struct ec_node_subset *node = (struct ec_node_subset *)gen_node;

	return __ec_node_subset_complete(node->table, node->len, strvec);
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
		ec_log(EC_LOG_ERR, "cannot create node\n");
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
		ec_log(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		"foo", "bar", "bar2", "toto", "titi", EC_NODE_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"bar", "bar2", "", EC_NODE_ENDLIST,
		"foo", "toto", "titi", EC_NODE_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"f", EC_NODE_ENDLIST,
		"oo", EC_NODE_ENDLIST,
		"oo");
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"b", EC_NODE_ENDLIST,
		"ar", "ar2", EC_NODE_ENDLIST,
		"ar");
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"bar", EC_NODE_ENDLIST,
		"", "2", EC_NODE_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"bar", "b", EC_NODE_ENDLIST,
		"ar2", EC_NODE_ENDLIST,
		"ar2");
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"t", EC_NODE_ENDLIST,
		"oto", "iti", EC_NODE_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"to", EC_NODE_ENDLIST,
		"to", EC_NODE_ENDLIST,
		"to");
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"x", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST,
		"");
	ec_node_free(node);

	return ret;
}

static struct ec_test ec_node_subset_test = {
	.name = "node_subset",
	.test = ec_node_subset_testcase,
};

EC_TEST_REGISTER(ec_node_subset_test);
