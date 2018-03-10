/*
 * Copyright (c) 2016, Olivier MATZ <zer0@droids-corp.org>
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

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_parsed.h>
#include <ecoli_completed.h>
#include <ecoli_node_str.h>
#include <ecoli_node_or.h>
#include <ecoli_node_many.h>
#include <ecoli_node_once.h>

EC_LOG_TYPE_REGISTER(node_once);

struct ec_node_once {
	struct ec_node gen;
	struct ec_node *child;
};

static unsigned int
count_node(struct ec_parsed *parsed, const struct ec_node *node)
{
	struct ec_parsed *child;
	unsigned int count = 0;

	if (parsed == NULL)
		return 0;

	if (ec_parsed_get_node(parsed) == node)
		count++;

	EC_PARSED_FOREACH_CHILD(child, parsed)
		count += count_node(child, node);

	return count;
}

static int
ec_node_once_parse(const struct ec_node *gen_node,
		struct ec_parsed *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_once *node = (struct ec_node_once *)gen_node;
	unsigned int count;

	/* count the number of occurences of the node: if already parsed,
	 * do not match
	 */
	count = count_node(ec_parsed_get_root(state), node->child);
	if (count > 0)
		return EC_PARSED_NOMATCH;

	return ec_node_parse_child(node->child, state, strvec);
}

static int
ec_node_once_complete(const struct ec_node *gen_node,
		struct ec_completed *completed,
		const struct ec_strvec *strvec)
{
	struct ec_node_once *node = (struct ec_node_once *)gen_node;
	struct ec_parsed *parsed = ec_completed_get_state(completed);
	unsigned int count;
	int ret;

	/* count the number of occurences of the node: if already parsed,
	 * do not match
	 */
	count = count_node(ec_parsed_get_root(parsed), node->child);
	if (count > 0)
		return 0;

	ret = ec_node_complete_child(node->child, completed, strvec);
	if (ret < 0)
		return ret;

	return 0;
}

static void ec_node_once_free_priv(struct ec_node *gen_node)
{
	struct ec_node_once *node = (struct ec_node_once *)gen_node;

	ec_node_free(node->child);
}

static struct ec_node_type ec_node_once_type = {
	.name = "once",
	.parse = ec_node_once_parse,
	.complete = ec_node_once_complete,
	.size = sizeof(struct ec_node_once),
	.free_priv = ec_node_once_free_priv,
};

EC_NODE_TYPE_REGISTER(ec_node_once_type);

int ec_node_once_set(struct ec_node *gen_node, struct ec_node *child)
{
	struct ec_node_once *node = (struct ec_node_once *)gen_node;

	if (gen_node == NULL || child == NULL) {
		errno = EINVAL;
		goto fail;
	}

	if (ec_node_check_type(gen_node, &ec_node_once_type) < 0)
		goto fail;

	if (ec_node_add_child(gen_node, child) < 0)
		goto fail;

	node->child = child;

	return 0;

fail:
	ec_node_free(child);
	return -1;
}

struct ec_node *ec_node_once(const char *id, struct ec_node *child)
{
	struct ec_node *gen_node = NULL;

	if (child == NULL)
		return NULL;

	gen_node = __ec_node(&ec_node_once_type, id);
	if (gen_node == NULL)
		goto fail;

	ec_node_once_set(gen_node, child);
	child = NULL;

	return gen_node;

fail:
	ec_node_free(child);
	return NULL;
}

/* LCOV_EXCL_START */
static int ec_node_once_testcase(void)
{
	struct ec_node *node;
	int testres = 0;

	node = ec_node_many(EC_NO_ID,
			EC_NODE_OR(EC_NO_ID,
				ec_node_once(EC_NO_ID, ec_node_str(EC_NO_ID, "foo")),
				ec_node_str(EC_NO_ID, "bar")
				), 0, 0
		);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 0);
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 2, "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 3, "foo", "bar", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 3, "bar", "foo", "bar");
	testres |= EC_TEST_CHECK_PARSE(node, 2, "bar", "foo", "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "foo", "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 0, "foox");

	testres |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		"foo", "bar", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"f", EC_NODE_ENDLIST,
		"foo", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"b", EC_NODE_ENDLIST,
		"bar", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"foo", "", EC_NODE_ENDLIST,
		"bar", EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"bar", "", EC_NODE_ENDLIST,
		"foo", "bar", EC_NODE_ENDLIST);
	ec_node_free(node);

	return testres;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_once_test = {
	.name = "node_once",
	.test = ec_node_once_testcase,
};

EC_TEST_REGISTER(ec_node_once_test);
