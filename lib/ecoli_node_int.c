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
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>

#include <ecoli_log.h>
#include <ecoli_malloc.h>
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_parsed.h>
#include <ecoli_completed.h>
#include <ecoli_node_int.h>
#include <ecoli_test.h>

EC_LOG_TYPE_REGISTER(node_int);

struct ec_node_int {
	struct ec_node gen;
	bool is_signed;
	bool check_min;
	bool check_max;
	union {
		int64_t min;
		uint64_t umin;
	};
	union {
		int64_t max;
		uint64_t umax;
	};
	unsigned int base;
};

static int parse_llint(struct ec_node_int *node, const char *str,
	int64_t *val)
{
	char *endptr;

	errno = 0;
	*val = strtoll(str, &endptr, node->base);

	if (errno == ERANGE && (*val == LLONG_MAX || *val == LLONG_MIN))
		return -1;

	if (errno != 0 && *val == 0)
		return -1;

	if (node->check_min && *val < node->min)
		return -1;

	if (node->check_max && *val > node->max)
		return -1;

	if (*endptr != 0)
		return -1;

	return 0;
}

static int parse_ullint(struct ec_node_int *node, const char *str,
			uint64_t *val)
{
	char *endptr;

	/* since a negative input is silently converted to a positive
	 * one by strtoull(), first check that it is positive */
	if (strchr(str, '-'))
		return -1;

	errno = 0;
	*val = strtoull(str, &endptr, node->base);

	if (errno == ERANGE && *val == ULLONG_MAX)
		return -1;

	if (errno != 0 && *val == 0)
		return -1;

	if (node->check_min && *val < node->umin)
		return -1;

	if (node->check_max && *val > node->umax)
		return -1;

	if (*endptr != 0)
		return -1;

	return 0;
}

static int ec_node_int_parse(const struct ec_node *gen_node,
			struct ec_parsed *state,
			const struct ec_strvec *strvec)
{
	struct ec_node_int *node = (struct ec_node_int *)gen_node;
	const char *str;
	int64_t val;

	(void)state;

	if (ec_strvec_len(strvec) == 0)
		return EC_PARSED_NOMATCH;

	str = ec_strvec_val(strvec, 0);
	if (parse_llint(node, str, &val) < 0)
		return EC_PARSED_NOMATCH;

	return 1;
}

static size_t ec_node_int_get_max_parse_len(const struct ec_node *gen_node)
{
	(void)gen_node;
	return 1;
}

static struct ec_node_type ec_node_int_type = {
	.name = "int",
	.parse = ec_node_int_parse,
	.complete = ec_node_default_complete,
	.get_max_parse_len = ec_node_int_get_max_parse_len,
	.size = sizeof(struct ec_node_int),
};

EC_NODE_TYPE_REGISTER(ec_node_int_type);

struct ec_node *ec_node_int(const char *id, int64_t min,
	int64_t max, unsigned int base)
{
	struct ec_node *gen_node = NULL;
	struct ec_node_int *node = NULL;

	if (min > max)
		return NULL;

	gen_node = __ec_node(&ec_node_int_type, id);
	if (gen_node == NULL)
		return NULL;
	node = (struct ec_node_int *)gen_node;

	node->check_min = true;
	node->min = min;
	node->check_max = true;
	node->max = max;
	node->base = base;
	node->is_signed = true;

	return &node->gen;
}

struct ec_node *ec_node_uint(const char *id, uint64_t min,
	uint64_t max, unsigned int base)
{
	struct ec_node *gen_node = NULL;
	struct ec_node_int *node = NULL;

	if (min > max)
		return NULL;

	gen_node = __ec_node(&ec_node_int_type, id);
	if (gen_node == NULL)
		return NULL;
	node = (struct ec_node_int *)gen_node;

	node->check_min = true;
	node->min = min;
	node->check_max = true;
	node->max = max;
	node->base = base;
	node->is_signed = true;

	return &node->gen;
}

int64_t ec_node_int_getval(struct ec_node *gen_node, const char *str)
{
	struct ec_node_int *node = (struct ec_node_int *)gen_node;
	int64_t val = 0;

	// XXX check type here
	// if gen_node->type != int fail
	// we may need to change the API (return int + val in a ptr ?)

	parse_llint(node, str, &val);

	return val;
}

uint64_t ec_node_uint_getval(struct ec_node *gen_node, const char *str)
{
	struct ec_node_int *node = (struct ec_node_int *)gen_node;
	uint64_t val = 0;

	// XXX check type here
	// if gen_node->type != int fail

	parse_ullint(node, str, &val);

	return val;
}

/* LCOV_EXCL_START */
static int ec_node_int_testcase(void)
{
	struct ec_parsed *p;
	struct ec_node *node;
	const char *s;
	int ret = 0;

	node = ec_node_uint(NULL, 0, 256, 0);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_PARSE(node, 1, "0");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "256", "foo");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "0x100");
	ret |= EC_TEST_CHECK_PARSE(node, 1, " 1");
	ret |= EC_TEST_CHECK_PARSE(node, -1, "-1");
	ret |= EC_TEST_CHECK_PARSE(node, -1, "0x101");

	p = ec_node_parse(node, "0");
	s = ec_strvec_val(ec_parsed_strvec(p), 0);
	EC_TEST_ASSERT(s != NULL && ec_node_int_getval(node, s) == 0);
	ec_parsed_free(p);

	p = ec_node_parse(node, "10");
	s = ec_strvec_val(ec_parsed_strvec(p), 0);
	EC_TEST_ASSERT(s != NULL && ec_node_uint_getval(node, s) == 10);
	ec_parsed_free(p);
	ec_node_free(node);

	node = ec_node_int(NULL, -1, LLONG_MAX, 16);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_PARSE(node, 1, "0");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "-1");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "7fffffffffffffff");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "0x7fffffffffffffff");
	ret |= EC_TEST_CHECK_PARSE(node, -1, "-2");

	p = ec_node_parse(node, "10");
	s = ec_strvec_val(ec_parsed_strvec(p), 0);
	EC_TEST_ASSERT(s != NULL && ec_node_int_getval(node, s) == 16);
	ec_parsed_free(p);
	ec_node_free(node);

	node = ec_node_int(NULL, LLONG_MIN, 0, 10);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_PARSE(node, 1, "0");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "-1");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "-9223372036854775808");
	ret |= EC_TEST_CHECK_PARSE(node, -1, "0x0");
	ret |= EC_TEST_CHECK_PARSE(node, -1, "1");
	ec_node_free(node);

	/* test completion */
	node = ec_node_int(NULL, 0, 10, 0);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"x", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"1", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	ec_node_free(node);

	return ret;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_int_test = {
	.name = "node_int",
	.test = ec_node_int_testcase,
};

EC_TEST_REGISTER(ec_node_int_test);
