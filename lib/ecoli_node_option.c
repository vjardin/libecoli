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

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_node_option.h>
#include <ecoli_node_str.h>
#include <ecoli_test.h>

struct ec_node_option {
	struct ec_node gen;
	struct ec_node *child;
};

static struct ec_parsed *ec_node_option_parse(const struct ec_node *gen_node,
	const struct ec_strvec *strvec)
{
	struct ec_node_option *node = (struct ec_node_option *)gen_node;
	struct ec_parsed *parsed = NULL, *child_parsed;
	struct ec_strvec *match_strvec;

	parsed = ec_parsed_new();
	if (parsed == NULL)
		goto fail;

	child_parsed = ec_node_parse_strvec(node->child, strvec);
	if (child_parsed == NULL)
		goto fail;

	if (ec_parsed_matches(child_parsed)) {
		ec_parsed_add_child(parsed, child_parsed);
		match_strvec = ec_strvec_dup(child_parsed->strvec);
	} else {
		ec_parsed_free(child_parsed);
		match_strvec = ec_strvec_new();
	}

	if (match_strvec == NULL)
		goto fail;

	ec_parsed_set_match(parsed, gen_node, match_strvec);

	return parsed;

 fail:
	ec_parsed_free(parsed);
	return NULL;
}

static struct ec_completed *ec_node_option_complete(const struct ec_node *gen_node,
	const struct ec_strvec *strvec)
{
	struct ec_node_option *node = (struct ec_node_option *)gen_node;

	return ec_node_complete_strvec(node->child, strvec);
}

static void ec_node_option_free_priv(struct ec_node *gen_node)
{
	struct ec_node_option *node = (struct ec_node_option *)gen_node;

	ec_node_free(node->child);
}

static struct ec_node_type ec_node_option_type = {
	.name = "option",
	.parse = ec_node_option_parse,
	.complete = ec_node_option_complete,
	.size = sizeof(struct ec_node_option),
	.free_priv = ec_node_option_free_priv,
};

EC_NODE_TYPE_REGISTER(ec_node_option_type);

struct ec_node *ec_node_option(const char *id, struct ec_node *child)
{
	struct ec_node *gen_node = NULL;
	struct ec_node_option *node = NULL;

	if (child == NULL)
		return NULL;

	gen_node = __ec_node_new(&ec_node_option_type, id);
	if (gen_node == NULL) {
		ec_node_free(child);
		return NULL;
	}
	node = (struct ec_node_option *)gen_node;

	node->child = child;

	child->parent = gen_node;
	TAILQ_INSERT_TAIL(&gen_node->children, child, next);

	return &node->gen;
}

static int ec_node_option_testcase(void)
{
	struct ec_node *node;
	int ret = 0;

	node = ec_node_option(NULL, ec_node_str(NULL, "foo"));
	if (node == NULL) {
		ec_log(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "foo", "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 0, "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 0);
	ec_node_free(node);

	/* test completion */
	node = ec_node_option(NULL, ec_node_str(NULL, "foo"));
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
		"b", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST,
		"");
	ec_node_free(node);

	return ret;
}

static struct ec_test ec_node_option_test = {
	.name = "node_option",
	.test = ec_node_option_testcase,
};

EC_TEST_REGISTER(ec_node_option_test);
