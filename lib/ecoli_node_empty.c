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

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_node_empty.h>

struct ec_node_empty {
	struct ec_node gen;
};

static struct ec_parsed *ec_node_empty_parse(const struct ec_node *gen_node,
	const struct ec_strvec *strvec)
{
	struct ec_parsed *parsed;
	struct ec_strvec *match_strvec;

	(void)strvec;

	parsed = ec_parsed_new();
	if (parsed == NULL)
		goto fail;

	match_strvec = ec_strvec_new();
	if (match_strvec == NULL)
		goto fail;

	ec_parsed_set_match(parsed, gen_node, match_strvec);

	return parsed;

 fail:
	ec_parsed_free(parsed);
	return NULL;
}

static struct ec_node_type ec_node_empty_type = {
	.name = "empty",
	.parse = ec_node_empty_parse,
	.complete = ec_node_default_complete,
	.size = sizeof(struct ec_node_empty),
};

EC_NODE_TYPE_REGISTER(ec_node_empty_type);

static int ec_node_empty_testcase(void)
{
	struct ec_node *node;
	int ret = 0;

	node = ec_node_new("empty", NULL);
	if (node == NULL) {
		ec_log(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_PARSE(node, 0, "foo");
	ret |= EC_TEST_CHECK_PARSE(node, 0);
	ret |= EC_TEST_CHECK_PARSE(node, 0, "foo", "bar");
	ec_node_free(node);

	/* never completes */
	node = ec_node_new("empty", NULL);
	if (node == NULL) {
		ec_log(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST,
		"");
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"foo", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST,
		"");
	ec_node_free(node);

	return ret;
}

static struct ec_test ec_node_empty_test = {
	.name = "node_empty",
	.test = ec_node_empty_testcase,
};

EC_TEST_REGISTER(ec_node_empty_test);