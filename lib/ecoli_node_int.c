/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
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
#include <ecoli_config.h>
#include <ecoli_parse.h>
#include <ecoli_complete.h>
#include <ecoli_node_int.h>
#include <ecoli_test.h>

EC_LOG_TYPE_REGISTER(node_int);

/* common to int and uint */
struct ec_node_int_uint {
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

static int parse_llint(struct ec_node_int_uint *node, const char *str,
	int64_t *val)
{
	char *endptr;
	int save_errno = errno;

	errno = 0;
	*val = strtoll(str, &endptr, node->base);

	if ((errno == ERANGE && (*val == LLONG_MAX || *val == LLONG_MIN)) ||
			(errno != 0 && *val == 0))
		return -1;

	if (node->check_min && *val < node->min) {
		errno = ERANGE;
		return -1;
	}

	if (node->check_max && *val > node->max) {
		errno = ERANGE;
		return -1;
	}

	if (*endptr != 0) {
		errno = EINVAL;
		return -1;
	}

	errno = save_errno;
	return 0;
}

static int parse_ullint(struct ec_node_int_uint *node, const char *str,
			uint64_t *val)
{
	char *endptr;
	int save_errno = errno;

	/* since a negative input is silently converted to a positive
	 * one by strtoull(), first check that it is positive */
	if (strchr(str, '-'))
		return -1;

	errno = 0;
	*val = strtoull(str, &endptr, node->base);

	if ((errno == ERANGE && *val == ULLONG_MAX) ||
			(errno != 0 && *val == 0))
		return -1;

	if (node->check_min && *val < node->umin)
		return -1;

	if (node->check_max && *val > node->umax)
		return -1;

	if (*endptr != 0)
		return -1;

	errno = save_errno;
	return 0;
}

static int ec_node_int_uint_parse(const struct ec_node *gen_node,
			struct ec_parse *state,
			const struct ec_strvec *strvec)
{
	struct ec_node_int_uint *node = (struct ec_node_int_uint *)gen_node;
	const char *str;
	uint64_t u64;
	int64_t i64;

	(void)state;

	if (ec_strvec_len(strvec) == 0)
		return EC_PARSE_NOMATCH;

	str = ec_strvec_val(strvec, 0);
	if (node->is_signed) {
		if (parse_llint(node, str, &i64) < 0)
			return EC_PARSE_NOMATCH;
	} else {
		if (parse_ullint(node, str, &u64) < 0)
			return EC_PARSE_NOMATCH;
	}
	return 1;
}

static int
ec_node_uint_init_priv(struct ec_node *gen_node)
{
	struct ec_node_int_uint *node = (struct ec_node_int_uint *)gen_node;

	node->is_signed = true;

	return 0;
}

static const struct ec_config_schema ec_node_int_schema[] = {
	{
		.key = "min",
		.desc = "The minimum valid value (included).",
		.type = EC_CONFIG_TYPE_INT64,
	},
	{
		.key = "max",
		.desc = "The maximum valid value (included).",
		.type = EC_CONFIG_TYPE_INT64,
	},
	{
		.key = "base",
		.desc = "The base to use. If unset or 0, try to guess.",
		.type = EC_CONFIG_TYPE_UINT64,
	},
};

static int ec_node_int_set_config(struct ec_node *gen_node,
				const struct ec_config *config)
{
	struct ec_node_int_uint *node = (struct ec_node_int_uint *)gen_node;
	const struct ec_config *min_value = NULL;
	const struct ec_config *max_value = NULL;
	const struct ec_config *base_value = NULL;
	char *s = NULL;

	min_value = ec_config_dict_get(config, "min");
	max_value = ec_config_dict_get(config, "max");
	base_value = ec_config_dict_get(config, "base");

	if (min_value && max_value && min_value->i64 > max_value->i64) {
		errno = EINVAL;
		goto fail;
	}

	if (min_value != NULL) {
		node->check_min = true;
		node->min = min_value->i64;
	} else {
		node->check_min = false;
	}
	if (max_value != NULL) {
		node->check_max = true;
		node->max = max_value->i64;
	} else {
		node->check_min = false;
	}
	if (base_value != NULL)
		node->base = base_value->u64;
	else
		node->base = 0;

	return 0;

fail:
	ec_free(s);
	return -1;
}

static struct ec_node_type ec_node_int_type = {
	.name = "int",
	.schema = ec_node_int_schema,
	.schema_len = EC_COUNT_OF(ec_node_int_schema),
	.set_config = ec_node_int_set_config,
	.parse = ec_node_int_uint_parse,
	.complete = ec_node_complete_unknown,
	.size = sizeof(struct ec_node_int_uint),
	.init_priv = ec_node_uint_init_priv,
};

EC_NODE_TYPE_REGISTER(ec_node_int_type);

struct ec_node *ec_node_int(const char *id, int64_t min,
	int64_t max, unsigned int base)
{
	struct ec_config *config = NULL;
	struct ec_node *gen_node = NULL;
	int ret;

	gen_node = ec_node_from_type(&ec_node_int_type, id);
	if (gen_node == NULL)
		return NULL;

	config = ec_config_dict();
	if (config == NULL)
		goto fail;

	ret = ec_config_dict_set(config, "min", ec_config_i64(min));
	if (ret < 0)
		goto fail;
	ret = ec_config_dict_set(config, "max", ec_config_i64(max));
	if (ret < 0)
		goto fail;
	ret = ec_config_dict_set(config, "base", ec_config_u64(base));
	if (ret < 0)
		goto fail;

	ret = ec_node_set_config(gen_node, config);
	config = NULL;
	if (ret < 0)
		goto fail;

	return gen_node;

fail:
	ec_config_free(config);
	ec_node_free(gen_node);
	return NULL;
}

static const struct ec_config_schema ec_node_uint_schema[] = {
	{
		.key = "min",
		.desc = "The minimum valid value (included).",
		.type = EC_CONFIG_TYPE_UINT64,
	},
	{
		.key = "max",
		.desc = "The maximum valid value (included).",
		.type = EC_CONFIG_TYPE_UINT64,
	},
	{
		.key = "base",
		.desc = "The base to use. If unset or 0, try to guess.",
		.type = EC_CONFIG_TYPE_UINT64,
	},
};

static int ec_node_uint_set_config(struct ec_node *gen_node,
				const struct ec_config *config)
{
	struct ec_node_int_uint *node = (struct ec_node_int_uint *)gen_node;
	const struct ec_config *min_value = NULL;
	const struct ec_config *max_value = NULL;
	const struct ec_config *base_value = NULL;
	char *s = NULL;

	min_value = ec_config_dict_get(config, "min");
	max_value = ec_config_dict_get(config, "max");
	base_value = ec_config_dict_get(config, "base");

	if (min_value && max_value && min_value->u64 > max_value->u64) {
		errno = EINVAL;
		goto fail;
	}

	if (min_value != NULL) {
		node->check_min = true;
		node->min = min_value->u64;
	} else {
		node->check_min = false;
	}
	if (max_value != NULL) {
		node->check_max = true;
		node->max = max_value->u64;
	} else {
		node->check_min = false;
	}
	if (base_value != NULL)
		node->base = base_value->u64;
	else
		node->base = 0;

	return 0;

fail:
	ec_free(s);
	return -1;
}

static struct ec_node_type ec_node_uint_type = {
	.name = "uint",
	.schema = ec_node_uint_schema,
	.schema_len = EC_COUNT_OF(ec_node_uint_schema),
	.set_config = ec_node_uint_set_config,
	.parse = ec_node_int_uint_parse,
	.complete = ec_node_complete_unknown,
	.size = sizeof(struct ec_node_int_uint),
};

EC_NODE_TYPE_REGISTER(ec_node_uint_type);

struct ec_node *ec_node_uint(const char *id, uint64_t min,
	uint64_t max, unsigned int base)
{
	struct ec_config *config = NULL;
	struct ec_node *gen_node = NULL;
	int ret;

	gen_node = ec_node_from_type(&ec_node_uint_type, id);
	if (gen_node == NULL)
		return NULL;

	config = ec_config_dict();
	if (config == NULL)
		goto fail;

	ret = ec_config_dict_set(config, "min", ec_config_u64(min));
	if (ret < 0)
		goto fail;
	ret = ec_config_dict_set(config, "max", ec_config_u64(max));
	if (ret < 0)
		goto fail;
	ret = ec_config_dict_set(config, "base", ec_config_u64(base));
	if (ret < 0)
		goto fail;

	ret = ec_node_set_config(gen_node, config);
	config = NULL;
	if (ret < 0)
		goto fail;

	return gen_node;

fail:
	ec_config_free(config);
	ec_node_free(gen_node);
	return NULL;
}

int ec_node_int_getval(const struct ec_node *gen_node, const char *str,
			int64_t *result)
{
	struct ec_node_int_uint *node = (struct ec_node_int_uint *)gen_node;
	int ret;

	ret = ec_node_check_type(gen_node, &ec_node_int_type);
	if (ret < 0)
		return ret;

	if (parse_llint(node, str, result) < 0)
		return -1;

	return 0;
}

int ec_node_uint_getval(const struct ec_node *gen_node, const char *str,
			uint64_t *result)
{
	struct ec_node_int_uint *node = (struct ec_node_int_uint *)gen_node;
	int ret;

	ret = ec_node_check_type(gen_node, &ec_node_uint_type);
	if (ret < 0)
		return ret;

	if (parse_ullint(node, str, result) < 0)
		return -1;

	return 0;
}

/* LCOV_EXCL_START */
static int ec_node_int_testcase(void)
{
	struct ec_parse *p;
	struct ec_node *node;
	const char *s;
	int testres = 0;
	uint64_t u64;
	int64_t i64;

	node = ec_node_uint(EC_NO_ID, 1, 256, 0);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, -1, "");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "0");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "1");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "256", "foo");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "0x100");
	testres |= EC_TEST_CHECK_PARSE(node, 1, " 1");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "-1");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "0x101");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "zzz");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "0x100000000000000000");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "4r");

	p = ec_node_parse(node, "1");
	s = ec_strvec_val(ec_parse_strvec(p), 0);
	testres |= EC_TEST_CHECK(s != NULL &&
		ec_node_uint_getval(node, s, &u64) == 0 &&
		u64 == 1, "bad integer value");
	ec_parse_free(p);

	p = ec_node_parse(node, "10");
	s = ec_strvec_val(ec_parse_strvec(p), 0);
	testres |= EC_TEST_CHECK(s != NULL &&
		ec_node_uint_getval(node, s, &u64) == 0 &&
		u64 == 10, "bad integer value");
	ec_parse_free(p);
	ec_node_free(node);

	node = ec_node_int(EC_NO_ID, -1, LLONG_MAX, 16);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 1, "0");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "-1");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "7fffffffffffffff");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "0x7fffffffffffffff");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "0x8000000000000000");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "-2");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "zzz");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "4r");

	p = ec_node_parse(node, "10");
	s = ec_strvec_val(ec_parse_strvec(p), 0);
	testres |= EC_TEST_CHECK(s != NULL &&
		ec_node_int_getval(node, s, &i64) == 0 &&
		i64 == 16, "bad integer value");
	ec_parse_free(p);
	ec_node_free(node);

	node = ec_node_int(EC_NO_ID, LLONG_MIN, 0, 10);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_PARSE(node, 1, "0");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "-1");
	testres |= EC_TEST_CHECK_PARSE(node, 1, "-9223372036854775808");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "0x0");
	testres |= EC_TEST_CHECK_PARSE(node, -1, "1");
	ec_node_free(node);

	/* test completion */
	node = ec_node_int(EC_NO_ID, 0, 10, 0);
	if (node == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"x", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	testres |= EC_TEST_CHECK_COMPLETE(node,
		"1", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	ec_node_free(node);

	return testres;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_int_test = {
	.name = "node_int",
	.test = ec_node_int_testcase,
};

EC_TEST_REGISTER(ec_node_int_test);
