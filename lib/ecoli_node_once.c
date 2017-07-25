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

	if (parsed->node == node)
		count++;

	TAILQ_FOREACH(child, &parsed->children, next)
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

static struct ec_completed *
ec_node_once_complete(const struct ec_node *gen_node,
		struct ec_parsed *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_once *node = (struct ec_node_once *)gen_node;
	struct ec_completed *completed = NULL, *child_completed = NULL;
	unsigned int count;

	completed = ec_completed();
	if (completed == NULL)
		goto fail;

	/* count the number of occurences of the node: if already parsed,
	 * do not match
	 */
	count = count_node(ec_parsed_get_root(state), node->child); //XXX
	if (count > 0)
		return completed;

	child_completed = ec_node_complete_child(node->child, state, strvec);
	if (child_completed == NULL)
		goto fail;

	ec_completed_merge(completed, child_completed);
	return completed;

fail:
	ec_completed_free(completed);
	return NULL;
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

	// XXX check node type

	assert(node != NULL);

	if (child == NULL)
		return -EINVAL;

	gen_node->flags &= ~EC_NODE_F_BUILT;

	node->child = child;

	child->parent = gen_node;
	TAILQ_INSERT_TAIL(&gen_node->children, child, next); // XXX really needed?

	return 0;
}

struct ec_node *ec_node_once(const char *id, struct ec_node *child)
{
	struct ec_node *gen_node = NULL;

	if (child == NULL)
		return NULL;

	gen_node = __ec_node(&ec_node_once_type, id);
	if (gen_node == NULL)
		return NULL;

	ec_node_once_set(gen_node, child);

	return gen_node;
}

/* LCOV_EXCL_START */
static int ec_node_once_testcase(void)
{
	struct ec_node *node;
	int ret = 0;

	node = ec_node_many(NULL,
			EC_NODE_OR(NULL,
				ec_node_once(NULL, ec_node_str(NULL, "foo")),
				ec_node_str(NULL, "bar")
				), 0, 0
		);
	if (node == NULL) {
		ec_log(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_PARSE(node, 0);
	ret |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 2, "foo", "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 3, "foo", "bar", "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 3, "bar", "foo", "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 2, "bar", "foo", "foo");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "foo", "foo");
	ret |= EC_TEST_CHECK_PARSE(node, 0, "foox");

	ec_node_free(node);

#if 0 //XXX no completion test for node_once
	/* test completion */
	node = EC_NODE_OR(NULL,
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
		"f", EC_NODE_ENDLIST,
		"oo", EC_NODE_ENDLIST,
		"oo");
	ec_node_free(node);
#endif
	return ret;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_once_test = {
	.name = "node_once",
	.test = ec_node_once_testcase,
};

EC_TEST_REGISTER(ec_node_once_test);
