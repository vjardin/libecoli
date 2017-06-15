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
#include <ecoli_node_weakref.h>

struct ec_node_weakref {
	struct ec_node gen;
	struct ec_node *child;
};

static struct ec_parsed *ec_node_weakref_parse(const struct ec_node *gen_node,
	const struct ec_strvec *strvec)
{
	struct ec_node_weakref *node = (struct ec_node_weakref *)gen_node;

	return ec_node_parse_strvec(node->child, strvec);
}

static struct ec_completed *ec_node_weakref_complete(const struct ec_node *gen_node,
	const struct ec_strvec *strvec)
{
	struct ec_node_weakref *node = (struct ec_node_weakref *)gen_node;

	return ec_node_complete_strvec(node->child, strvec);
}

static struct ec_node_type ec_node_weakref_type = {
	.name = "weakref",
	.parse = ec_node_weakref_parse,
	.complete = ec_node_weakref_complete,
	.size = sizeof(struct ec_node_weakref),
};

EC_NODE_TYPE_REGISTER(ec_node_weakref_type);

int ec_node_weakref_set(struct ec_node *gen_node, struct ec_node *child)
{
	struct ec_node_weakref *node = (struct ec_node_weakref *)gen_node;

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

struct ec_node *ec_node_weakref(const char *id, struct ec_node *child)
{
	struct ec_node *gen_node = NULL;

	if (child == NULL)
		return NULL;

	gen_node = __ec_node_new(&ec_node_weakref_type, id);
	if (gen_node == NULL)
		return NULL;

	ec_node_weakref_set(gen_node, child);

	return gen_node;
}

static int ec_node_weakref_testcase(void)
{
	return 0;
}

static struct ec_test ec_node_weakref_test = {
	.name = "node_weakref",
	.test = ec_node_weakref_testcase,
};

EC_TEST_REGISTER(ec_node_weakref_test);
