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
#include <ecoli_node_str.h>
#include <ecoli_node_option.h>
#include <ecoli_node_seq.h>

struct ec_node_seq {
	struct ec_node gen;
	struct ec_node **table;
	unsigned int len;
};

static struct ec_parsed *ec_node_seq_parse(const struct ec_node *gen_node,
	const struct ec_strvec *strvec)
{
	struct ec_node_seq *node = (struct ec_node_seq *)gen_node;
	struct ec_parsed *parsed, *child_parsed;
	struct ec_strvec *match_strvec;
	struct ec_strvec *childvec = NULL;
	size_t len = 0;
	unsigned int i;

	parsed = ec_parsed_new();
	if (parsed == NULL)
		goto fail;

	for (i = 0; i < node->len; i++) {
		childvec = ec_strvec_ndup(strvec, len,
			ec_strvec_len(strvec) - len);
		if (childvec == NULL)
			goto fail;

		child_parsed = ec_node_parse_strvec(node->table[i], childvec);
		if (child_parsed == NULL)
			goto fail;

		ec_strvec_free(childvec);
		childvec = NULL;

		if (!ec_parsed_matches(child_parsed)) {
			ec_parsed_free(child_parsed);
			// XXX ec_parsed_free_children needed? see subset.c
			ec_parsed_free_children(parsed);
			return parsed;
		}

		ec_parsed_add_child(parsed, child_parsed);
		len += ec_parsed_len(child_parsed);
	}

	match_strvec = ec_strvec_ndup(strvec, 0, len);
	if (match_strvec == NULL)
		goto fail;

	ec_parsed_set_match(parsed, gen_node, match_strvec);

	return parsed;

fail:
	ec_strvec_free(childvec);
	ec_parsed_free(parsed);
	return NULL;
}

static struct ec_completed *ec_node_seq_complete(const struct ec_node *gen_node,
	const struct ec_strvec *strvec)
{
	struct ec_node_seq *node = (struct ec_node_seq *)gen_node;
	struct ec_completed *completed, *child_completed;
	struct ec_strvec *childvec = NULL;
	struct ec_parsed *parsed;
	size_t len = 0;
	unsigned int i;

	completed = ec_completed_new();
	if (completed == NULL)
		return NULL;

	if (node->len == 0)
		return completed;

	for (i = 0; i < node->len && len < ec_strvec_len(strvec); i++) {
		childvec = ec_strvec_ndup(strvec, len,
			ec_strvec_len(strvec) - len);
		if (childvec == NULL)
			goto fail;

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

static void ec_node_seq_free_priv(struct ec_node *gen_node)
{
	struct ec_node_seq *node = (struct ec_node_seq *)gen_node;
	unsigned int i;

	for (i = 0; i < node->len; i++)
		ec_node_free(node->table[i]);
	ec_free(node->table);
}

static struct ec_node_type ec_node_seq_type = {
	.name = "seq",
	.parse = ec_node_seq_parse,
	.complete = ec_node_seq_complete,
	.size = sizeof(struct ec_node_seq),
	.free_priv = ec_node_seq_free_priv,
};

EC_NODE_TYPE_REGISTER(ec_node_seq_type);

int ec_node_seq_add(struct ec_node *gen_node, struct ec_node *child)
{
	struct ec_node_seq *node = (struct ec_node_seq *)gen_node;
	struct ec_node **table;

	// XXX check node type

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
	TAILQ_INSERT_TAIL(&gen_node->children, child, next); // XXX really needed?

	return 0;
}

struct ec_node *__ec_node_seq(const char *id, ...)
{
	struct ec_node *gen_node = NULL;
	struct ec_node_seq *node = NULL;
	struct ec_node *child;
	va_list ap;
	int fail = 0;

	va_start(ap, id);

	gen_node = __ec_node_new(&ec_node_seq_type, id);
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

static int ec_node_seq_testcase(void)
{
	struct ec_node *node;
	int ret = 0;

	node = EC_NODE_SEQ(NULL,
		ec_node_str(NULL, "foo"),
		ec_node_str(NULL, "bar")
	);
	if (node == NULL) {
		ec_log(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_PARSE(node, 2, "foo", "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 2, "foo", "bar", "toto");
	ret |= EC_TEST_CHECK_PARSE(node, -1, "foo");
	ret |= EC_TEST_CHECK_PARSE(node, -1, "foox", "bar");
	ret |= EC_TEST_CHECK_PARSE(node, -1, "foo", "barx");
	ret |= EC_TEST_CHECK_PARSE(node, -1, "bar", "foo");
	ret |= EC_TEST_CHECK_PARSE(node, -1, "", "foo");
	ec_node_free(node);

	/* test completion */
	node = EC_NODE_SEQ(NULL,
		ec_node_str(NULL, "foo"),
		ec_node_option(NULL, ec_node_str(NULL, "toto")),
		ec_node_str(NULL, "bar")
	);
	if (node == NULL) {
		ec_log(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		"foo", EC_NODE_ENDLIST,
		"foo");
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"f", EC_NODE_ENDLIST,
		"oo", EC_NODE_ENDLIST,
		"oo");
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"foo", EC_NODE_ENDLIST,
		"", EC_NODE_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"foo", "", EC_NODE_ENDLIST,
		"bar", "toto", EC_NODE_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"foo", "t", EC_NODE_ENDLIST,
		"oto", EC_NODE_ENDLIST,
		"oto");
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"foo", "b", EC_NODE_ENDLIST,
		"ar", EC_NODE_ENDLIST,
		"ar");
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"foo", "bar", EC_NODE_ENDLIST,
		"", EC_NODE_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"x", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"foobarx", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST,
		"");
	ec_node_free(node);

	return ret;
}

static struct ec_test ec_node_seq_test = {
	.name = "node_seq",
	.test = ec_node_seq_testcase,
};

EC_TEST_REGISTER(ec_node_seq_test);
