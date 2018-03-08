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
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_parsed.h>
#include <ecoli_completed.h>
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
		struct ec_parsed *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_or *node = (struct ec_node_or *)gen_node;
	unsigned int i;
	int ret;

	for (i = 0; i < node->len; i++) {
		ret = ec_node_parse_child(node->table[i], state, strvec);
		if (ret == EC_PARSED_NOMATCH)
			continue;
		return ret;
	}

	return EC_PARSED_NOMATCH;
}

static int
ec_node_or_complete(const struct ec_node *gen_node,
		struct ec_completed *completed,
		const struct ec_strvec *strvec)
{
	struct ec_node_or *node = (struct ec_node_or *)gen_node;
	int ret;
	size_t n;

	for (n = 0; n < node->len; n++) {
		ret = ec_node_complete_child(node->table[n],
					completed, strvec);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static size_t ec_node_or_get_max_parse_len(const struct ec_node *gen_node)
{
	struct ec_node_or *node = (struct ec_node_or *)gen_node;
	size_t i, ret = 0, len;

	for (i = 0; i < node->len; i++) {
		len = ec_node_get_max_parse_len(node->table[i]);
		if (len > ret)
			ret = len;
	}

	return ret;
}

static void ec_node_or_free_priv(struct ec_node *gen_node)
{
	struct ec_node_or *node = (struct ec_node_or *)gen_node;
	unsigned int i;

	for (i = 0; i < node->len; i++)
		ec_node_free(node->table[i]);
	ec_free(node->table);
}

int ec_node_or_add(struct ec_node *gen_node, struct ec_node *child)
{
	struct ec_node_or *node = (struct ec_node_or *)gen_node;
	struct ec_node **table;

	assert(node != NULL);

	if (child == NULL)
		return -EINVAL;

	table = ec_realloc(node->table, (node->len + 1) * sizeof(*node->table));
	if (table == NULL)
		return -1;

	node->table = table;
	table[node->len] = child;
	node->len++;

	TAILQ_INSERT_TAIL(&gen_node->children, child, next);

	return 0;
}

static struct ec_node_type ec_node_or_type = {
	.name = "or",
	.parse = ec_node_or_parse,
	.complete = ec_node_or_complete,
	.get_max_parse_len = ec_node_or_get_max_parse_len,
	.size = sizeof(struct ec_node_or),
	.free_priv = ec_node_or_free_priv,
};

EC_NODE_TYPE_REGISTER(ec_node_or_type);

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
	int ret = 0;

	node = EC_NODE_OR(EC_NO_ID,
		ec_node_str(EC_NO_ID, "foo"),
		ec_node_str(EC_NO_ID, "bar")
	);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "foo", "bar");
	ret |= EC_TEST_CHECK_PARSE(node, -1, " ");
	ret |= EC_TEST_CHECK_PARSE(node, -1, "foox");
	ret |= EC_TEST_CHECK_PARSE(node, -1, "toto");
	ret |= EC_TEST_CHECK_PARSE(node, -1, "");
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
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		"foo", "bar", "bar2", "toto", "titi", EC_NODE_ENDLIST);
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

static struct ec_test ec_node_or_test = {
	.name = "node_or",
	.test = ec_node_or_testcase,
};

EC_TEST_REGISTER(ec_node_or_test);
