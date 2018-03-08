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
#include <errno.h>
#include <regex.h>

#include <ecoli_log.h>
#include <ecoli_malloc.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_parsed.h>
#include <ecoli_completed.h>
#include <ecoli_node_re.h>

EC_LOG_TYPE_REGISTER(node_re);

struct ec_node_re {
	struct ec_node gen;
	char *re_str;
	regex_t re;
};

static int
ec_node_re_parse(const struct ec_node *gen_node,
		struct ec_parsed *state,
		const struct ec_strvec *strvec)
{
	struct ec_node_re *node = (struct ec_node_re *)gen_node;
	const char *str;
	regmatch_t pos;

	(void)state;

	if (ec_strvec_len(strvec) == 0)
		return EC_PARSED_NOMATCH;

	str = ec_strvec_val(strvec, 0);
	if (regexec(&node->re, str, 1, &pos, 0) != 0)
		return EC_PARSED_NOMATCH;
	if (pos.rm_so != 0 || pos.rm_eo != (int)strlen(str))
		return EC_PARSED_NOMATCH;

	return 1;
}

static void ec_node_re_free_priv(struct ec_node *gen_node)
{
	struct ec_node_re *node = (struct ec_node_re *)gen_node;

	ec_free(node->re_str);
	regfree(&node->re);
}

static struct ec_node_type ec_node_re_type = {
	.name = "re",
	.parse = ec_node_re_parse,
	.complete = ec_node_default_complete,
	.size = sizeof(struct ec_node_re),
	.free_priv = ec_node_re_free_priv,
};

EC_NODE_TYPE_REGISTER(ec_node_re_type);

int ec_node_re_set_regexp(struct ec_node *gen_node, const char *str)
{
	struct ec_node_re *node = (struct ec_node_re *)gen_node;
	int ret;

	if (str == NULL)
		return -EINVAL;
	if (node->re_str != NULL) // XXX allow to replace
		return -EEXIST;

	ret = -ENOMEM;
	node->re_str = ec_strdup(str);
	if (node->re_str == NULL)
		goto fail;

	ret = -EINVAL;
	if (regcomp(&node->re, node->re_str, REG_EXTENDED) != 0)
		goto fail;

	return 0;

fail:
	ec_free(node->re_str);
	return ret;
}

struct ec_node *ec_node_re(const char *id, const char *re_str)
{
	struct ec_node *gen_node = NULL;

	gen_node = __ec_node(&ec_node_re_type, id);
	if (gen_node == NULL)
		goto fail;

	if (ec_node_re_set_regexp(gen_node, re_str) < 0)
		goto fail;

	return gen_node;

fail:
	ec_node_free(gen_node);
	return NULL;
}

/* LCOV_EXCL_START */
static int ec_node_re_testcase(void)
{
	struct ec_node *node;
	int ret = 0;

	node = ec_node_re(EC_NO_ID, "fo+|bar");
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "foo", "bar");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "bar");
	ret |= EC_TEST_CHECK_PARSE(node, -1, "foobar");
	ret |= EC_TEST_CHECK_PARSE(node, -1, " foo");
	ret |= EC_TEST_CHECK_PARSE(node, -1, "");
	ec_node_free(node);

	return ret;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_re_test = {
	.name = "node_re",
	.test = ec_node_re_testcase,
};

EC_TEST_REGISTER(ec_node_re_test);
