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
#include <ecoli_node_option.h>
#include <ecoli_node_many.h>

struct ec_node_many {
	struct ec_node gen;
	unsigned int min;
	unsigned int max;
	struct ec_node *child;
};

static int ec_node_many_parse(const struct ec_node *gen_node,
			struct ec_parsed *state,
			const struct ec_strvec *strvec)
{
	struct ec_node_many *node = (struct ec_node_many *)gen_node;
	struct ec_parsed *child_parsed;
	struct ec_strvec *childvec = NULL;
	size_t off = 0, count;
	int ret;

	for (count = 0; node->max == 0 || count < node->max; count++) {
		childvec = ec_strvec_ndup(strvec, off,
			ec_strvec_len(strvec) - off);
		if (childvec == NULL) {
			ret = -ENOMEM;
			goto fail;
		}

		ret = ec_node_parse_child(node->child, state, childvec);
		ec_strvec_free(childvec);
		childvec = NULL;

		if (ret == EC_PARSED_NOMATCH)
			break;
		else if (ret < 0)
			goto fail;

		/* it matches an empty strvec, no need to continue */
		if (ret == 0) {
			child_parsed = ec_parsed_get_last_child(state);
			ec_parsed_del_child(state, child_parsed);
			ec_parsed_free(child_parsed);
			break;
		}

		off += ret;
	}

	if (count < node->min) {
		ec_parsed_free_children(state);
		return EC_PARSED_NOMATCH;
	}

	return off;

fail:
	ec_strvec_free(childvec);
	return ret;
}

#if 0 //XXX missing node_many_complete
static struct ec_completed *ec_node_many_complete(const struct ec_node *gen_node,
	const struct ec_strvec *strvec)
{
	struct ec_node_many *node = (struct ec_node_many *)gen_node;
	struct ec_completed *completed, *child_completed;
	struct ec_strvec *childvec;
	struct ec_parsed *parsed;
	size_t len = 0;
	unsigned int i;

	completed = ec_completed();
	if (completed == NULL)
		return NULL;

	if (node->len == 0)
		return completed;

	for (i = 0; i < node->len; i++) {
		childvec = ec_strvec_ndup(strvec, len,
			ec_strvec_len(strvec) - len);
		if (childvec == NULL)
			goto fail; // XXX fail ?

		child_completed = ec_node_complete_strvec(node->table[i],
			childvec);
		if (child_completed == NULL)
			goto fail;

		ec_completed_merge(completed, child_completed);

		parsed = ec_node_parse_strvec(node->table[i], childvec);
		if (parsed == NULL)
			goto fail;

		ec_strvec_free(childvec);
		childvec = NULL;

		if (!ec_parsed_matches(parsed)) {
			ec_parsed_free(parsed);
			break;
		}

		len += ec_strvec_len(parsed->strvec);
		ec_parsed_free(parsed);
	}

	return completed;

fail:
	ec_strvec_free(childvec);
	ec_completed_free(completed);
	return NULL;
}
#endif

static void ec_node_many_free_priv(struct ec_node *gen_node)
{
	struct ec_node_many *node = (struct ec_node_many *)gen_node;

	ec_node_free(node->child);
}

static struct ec_node_type ec_node_many_type = {
	.name = "many",
	.parse = ec_node_many_parse,
	.complete = ec_node_default_complete,
//XXX	.complete = ec_node_many_complete,
	.size = sizeof(struct ec_node_many),
	.free_priv = ec_node_many_free_priv,
};

EC_NODE_TYPE_REGISTER(ec_node_many_type);

struct ec_node *ec_node_many(const char *id, struct ec_node *child,
	unsigned int min, unsigned int max)
{
	struct ec_node_many *node = NULL;

	if (child == NULL)
		return NULL;

	node = (struct ec_node_many *)__ec_node(&ec_node_many_type, id);
	if (node == NULL) {
		ec_node_free(child);
		return NULL;
	}

	node->child = child;
	node->min = min;
	node->max = max;

	return &node->gen;
}

static int ec_node_many_testcase(void)
{
	struct ec_node *node;
	int ret = 0;

	node = ec_node_many(NULL, ec_node_str(NULL, "foo"), 0, 0);
	if (node == NULL) {
		ec_log(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_PARSE(node, 0);
	ret |= EC_TEST_CHECK_PARSE(node, 0, "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "foo", "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 2, "foo", "foo", "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 0);
	ec_node_free(node);

	node = ec_node_many(NULL, ec_node_str(NULL, "foo"), 1, 0);
	if (node == NULL) {
		ec_log(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_PARSE(node, -1, "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "foo", "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 2, "foo", "foo", "bar");
	ret |= EC_TEST_CHECK_PARSE(node, -1);
	ec_node_free(node);

	node = ec_node_many(NULL, ec_node_str(NULL, "foo"), 1, 2);
	if (node == NULL) {
		ec_log(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_PARSE(node, -1, "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "foo", "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 2, "foo", "foo", "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 2, "foo", "foo", "foo");
	ret |= EC_TEST_CHECK_PARSE(node, -1);
	ec_node_free(node);

	/* test completion */
	/* XXX */

	return ret;
}

static struct ec_test ec_node_many_test = {
	.name = "many",
	.test = ec_node_many_testcase,
};

EC_TEST_REGISTER(ec_node_many_test);
