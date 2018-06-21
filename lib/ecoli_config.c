/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018, Olivier MATZ <zer0@droids-corp.org>
 */

#include <sys/queue.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <inttypes.h>

#include <ecoli_string.h>
#include <ecoli_malloc.h>
#include <ecoli_keyval.h>
#include <ecoli_node.h>
#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_config.h>

EC_LOG_TYPE_REGISTER(config);

static int
__ec_config_dump(FILE *out, const char *key, const struct ec_config *config,
	size_t indent);
static int
ec_config_dict_validate(const struct ec_keyval *dict,
			const struct ec_config_schema *schema,
			size_t schema_len);

/* return ec_value type as a string */
static const char *
ec_config_type_str(enum ec_config_type type)
{
	switch (type) {
	case EC_CONFIG_TYPE_BOOL: return "bool";
	case EC_CONFIG_TYPE_INT64: return "int64";
	case EC_CONFIG_TYPE_UINT64: return "uint64";
	case EC_CONFIG_TYPE_STRING: return "string";
	case EC_CONFIG_TYPE_NODE: return "node";
	case EC_CONFIG_TYPE_LIST: return "list";
	case EC_CONFIG_TYPE_DICT: return "dict";
	default: return "unknown";
	}
}

static int
__ec_config_schema_validate(const struct ec_config_schema *schema,
			size_t schema_len, enum ec_config_type type)
{
	size_t i, j;
	int ret;

	if (type == EC_CONFIG_TYPE_LIST) {
		if (schema[0].key != NULL) {
			errno = EINVAL;
			EC_LOG(EC_LOG_ERR, "list schema key must be NULL\n");
			return -1;
		}
	} else if (type == EC_CONFIG_TYPE_DICT) {
		for (i = 0; i < schema_len; i++) {
			if (schema[i].key == NULL) {
				errno = EINVAL;
				EC_LOG(EC_LOG_ERR,
					"dict schema key should not be NULL\n");
				return -1;
			}
		}
	} else {
		errno = EINVAL;
		EC_LOG(EC_LOG_ERR, "invalid schema type\n");
		return -1;
	}

	for (i = 0; i < schema_len; i++) {
		/* check for duplicate name if more than one element */
		for (j = i + 1; j < schema_len; j++) {
			if (!strcmp(schema[i].key, schema[j].key)) {
				errno = EEXIST;
				EC_LOG(EC_LOG_ERR,
					"duplicate key <%s> in schema\n",
					schema[i].key);
				return -1;
			}
		}

		switch (schema[i].type) {
		case EC_CONFIG_TYPE_BOOL:
		case EC_CONFIG_TYPE_INT64:
		case EC_CONFIG_TYPE_UINT64:
		case EC_CONFIG_TYPE_STRING:
		case EC_CONFIG_TYPE_NODE:
			if (schema[i].subschema != NULL ||
					schema[i].subschema_len != 0) {
				errno = EINVAL;
				EC_LOG(EC_LOG_ERR,
					"key <%s> should not have subtype/subschema\n",
					schema[i].key);
				return -1;
			}
			break;
		case EC_CONFIG_TYPE_LIST:
			if (schema[i].subschema == NULL ||
					schema[i].subschema_len != 1) {
				errno = EINVAL;
				EC_LOG(EC_LOG_ERR,
					"key <%s> must have subschema of length 1\n",
					schema[i].key);
				return -1;
			}
			break;
		case EC_CONFIG_TYPE_DICT:
			if (schema[i].subschema == NULL ||
					schema[i].subschema_len == 0) {
				errno = EINVAL;
				EC_LOG(EC_LOG_ERR,
					"key <%s> must have subschema\n",
					schema[i].key);
				return -1;
			}
			break;
		default:
			EC_LOG(EC_LOG_ERR, "invalid type for key <%s>\n",
				schema[i].key);
			errno = EINVAL;
			return -1;
		}

		if (schema[i].subschema == NULL)
			continue;

		ret = __ec_config_schema_validate(schema[i].subschema,
						schema[i].subschema_len,
						schema[i].type);
		if (ret < 0) {
			EC_LOG(EC_LOG_ERR, "cannot parse subschema %s%s\n",
				schema[i].key ? "key=" : "",
				schema[i].key ? : "");
			return ret;
		}
	}

	return 0;
}

int
ec_config_schema_validate(const struct ec_config_schema *schema,
			size_t schema_len)
{
	return __ec_config_schema_validate(schema, schema_len,
					EC_CONFIG_TYPE_DICT);
}

static void
__ec_config_schema_dump(FILE *out, const struct ec_config_schema *schema,
			size_t schema_len, size_t indent)
{
	size_t i;

	for (i = 0; i < schema_len; i++) {
		fprintf(out, "%*s" "%s%s%stype=%s desc='%s'\n",
			(int)indent * 4, "",
			schema[i].key ? "key=": "",
			schema[i].key ? : "",
			schema[i].key ? " ": "",
			ec_config_type_str(schema[i].type),
			schema[i].desc);
		if (schema[i].subschema == NULL)
			continue;
		__ec_config_schema_dump(out, schema[i].subschema,
					schema[i].subschema_len, indent + 1);
	}
}

void
ec_config_schema_dump(FILE *out, const struct ec_config_schema *schema,
		size_t schema_len)
{
	fprintf(out, "------------------- schema dump:\n");

	if (schema == NULL || schema_len == 0) {
		fprintf(out, "no schema\n");
		return;
	}

	__ec_config_schema_dump(out, schema, schema_len, 0);
}

struct ec_config *
ec_config_bool(bool boolean)
{
	struct ec_config *value = NULL;

	value = ec_calloc(1, sizeof(*value));
	if (value == NULL)
		return NULL;

	value->type = EC_CONFIG_TYPE_BOOL;
	value->boolean = boolean;

	return value;
}

struct ec_config *
ec_config_i64(int64_t i64)
{
	struct ec_config *value = NULL;

	value = ec_calloc(1, sizeof(*value));
	if (value == NULL)
		return NULL;

	value->type = EC_CONFIG_TYPE_INT64;
	value->i64 = i64;

	return value;
}

struct ec_config *
ec_config_u64(uint64_t u64)
{
	struct ec_config *value = NULL;

	value = ec_calloc(1, sizeof(*value));
	if (value == NULL)
		return NULL;

	value->type = EC_CONFIG_TYPE_UINT64;
	value->u64 = u64;

	return value;
}

/* duplicate string */
struct ec_config *
ec_config_string(const char *string)
{
	struct ec_config *value = NULL;
	char *s = NULL;

	if (string == NULL)
		goto fail;

	s = ec_strdup(string);
	if (s == NULL)
		goto fail;

	value = ec_calloc(1, sizeof(*value));
	if (value == NULL)
		goto fail;

	value->type = EC_CONFIG_TYPE_STRING;
	value->string = s;

	return value;

fail:
	ec_free(value);
	ec_free(s);
	return NULL;
}

/* "consume" the node */
struct ec_config *
ec_config_node(struct ec_node *node)
{
	struct ec_config *value = NULL;

	if (node == NULL)
		goto fail;

	value = ec_calloc(1, sizeof(*value));
	if (value == NULL)
		goto fail;

	value->type = EC_CONFIG_TYPE_NODE;
	value->node = node;

	return value;

fail:
	ec_node_free(node);
	ec_free(value);
	return NULL;
}

struct ec_config *
ec_config_dict(void)
{
	struct ec_config *value = NULL;
	struct ec_keyval *dict = NULL;

	dict = ec_keyval();
	if (dict == NULL)
		goto fail;

	value = ec_calloc(1, sizeof(*value));
	if (value == NULL)
		goto fail;

	value->type = EC_CONFIG_TYPE_DICT;
	value->dict = dict;

	return value;

fail:
	ec_keyval_free(dict);
	ec_free(value);
	return NULL;
}

struct ec_config *
ec_config_list(void)
{
	struct ec_config *value = NULL;

	value = ec_calloc(1, sizeof(*value));
	if (value == NULL)
		return NULL;

	value->type = EC_CONFIG_TYPE_LIST;
	TAILQ_INIT(&value->list);

	return value;
}

static const struct ec_config_schema *
ec_config_schema_lookup(const struct ec_config_schema *schema,
			size_t schema_len, const char *key,
			enum ec_config_type type)
{
	size_t i;

	for (i = 0; i < schema_len; i++) {
		if (!strcmp(key, schema[i].key) &&
				type == schema[i].type)
			return &schema[i];
	}

	errno = ENOENT;
	return NULL;
}

void
ec_config_free(struct ec_config *value)
{
	if (value == NULL)
		return;

	switch (value->type) {
	case EC_CONFIG_TYPE_STRING:
		ec_free(value->string);
		break;
	case EC_CONFIG_TYPE_NODE:
		ec_node_free(value->node);
		break;
	case EC_CONFIG_TYPE_LIST:
		while (!TAILQ_EMPTY(&value->list)) {
			struct ec_config *v;
			v = TAILQ_FIRST(&value->list);
			TAILQ_REMOVE(&value->list, v, next);
			ec_config_free(v);
		}
		break;
	case EC_CONFIG_TYPE_DICT:
		ec_keyval_free(value->dict);
		break;
	default:
		break;
	}

	ec_free(value);
}

static int
ec_config_list_cmp(const struct ec_config_list *list1,
		const struct ec_config_list *list2)
{
	const struct ec_config *v1, *v2;

	for (v1 = TAILQ_FIRST(list1), v2 = TAILQ_FIRST(list2);
	     v1 != NULL && v2 != NULL;
	     v1 = TAILQ_NEXT(v1, next), v2 = TAILQ_NEXT(v2, next)) {
		if (ec_config_cmp(v1, v2))
			return -1;
	}
	if (v1 != NULL || v2 != NULL)
		return -1;

	return 0;
}

/* XXX -> ec_keyval_cmp() */
static int
ec_config_dict_cmp(const struct ec_keyval *d1,
		const struct ec_keyval *d2)
{
	const struct ec_config *v1, *v2;
	struct ec_keyval_iter *iter = NULL;
	const char *key;

	if (ec_keyval_len(d1) != ec_keyval_len(d2))
		return -1;

	for (iter = ec_keyval_iter(d1);
	     ec_keyval_iter_valid(iter);
	     ec_keyval_iter_next(iter)) {
		key = ec_keyval_iter_get_key(iter);
		v1 = ec_keyval_iter_get_val(iter);
		v2 = ec_keyval_get(d2, key);

		if (ec_config_cmp(v1, v2))
			goto fail;
	}

	ec_keyval_iter_free(iter);
	return 0;

fail:
	ec_keyval_iter_free(iter);
	return -1;
}

int
ec_config_cmp(const struct ec_config *value1,
		const struct ec_config *value2)
{
	if (value1->type != value2->type)
		return -1;

	switch (value1->type) {
	case EC_CONFIG_TYPE_BOOL:
		if (value1->boolean == value2->boolean)
			return 0;
	case EC_CONFIG_TYPE_INT64:
		if (value1->i64 == value2->i64)
			return 0;
	case EC_CONFIG_TYPE_UINT64:
		if (value1->u64 == value2->u64)
			return 0;
	case EC_CONFIG_TYPE_STRING:
		if (!strcmp(value1->string, value2->string))
			return 0;
	case EC_CONFIG_TYPE_NODE:
		if (value1->node == value2->node)
			return 0;
	case EC_CONFIG_TYPE_LIST:
		return ec_config_list_cmp(&value1->list, &value2->list);
	case EC_CONFIG_TYPE_DICT:
		return ec_config_dict_cmp(value1->dict, value2->dict);
	default:
		break;
	}

	return -1;
}

static int
ec_config_list_validate(const struct ec_config_list *list,
			const struct ec_config_schema *sch)
{
	const struct ec_config *value;

	TAILQ_FOREACH(value, list, next) {
		if (value->type != sch->type) {
			errno = EBADMSG;
			return -1;
		}

		if (value->type == EC_CONFIG_TYPE_LIST) {
			if (ec_config_list_validate(&value->list,
							sch->subschema) < 0)
				return -1;
		} else if (value->type == EC_CONFIG_TYPE_DICT) {
			if (ec_config_dict_validate(value->dict,
					sch->subschema, sch->subschema_len) < 0)
				return -1;
		}
	}

	return 0;
}

static int
ec_config_dict_validate(const struct ec_keyval *dict,
			const struct ec_config_schema *schema,
			size_t schema_len)
{
	const struct ec_config *value;
	struct ec_keyval_iter *iter = NULL;
	const struct ec_config_schema *sch;
	const char *key;

	for (iter = ec_keyval_iter(dict);
	     ec_keyval_iter_valid(iter);
	     ec_keyval_iter_next(iter)) {

		key = ec_keyval_iter_get_key(iter);
		value = ec_keyval_iter_get_val(iter);
		sch = ec_config_schema_lookup(schema, schema_len,
					key, value->type);
		if (sch == NULL) {
			errno = EBADMSG;
			goto fail;
		}

		if (value->type == EC_CONFIG_TYPE_LIST) {
			if (ec_config_list_validate(&value->list,
							sch->subschema) < 0)
				goto fail;
		} else if (value->type == EC_CONFIG_TYPE_DICT) {
			if (ec_config_dict_validate(value->dict,
					sch->subschema, sch->subschema_len) < 0)
				goto fail;
		}
	}

	ec_keyval_iter_free(iter);
	return 0;

fail:
	ec_keyval_iter_free(iter);
	return -1;
}

int
ec_config_validate(const struct ec_config *dict,
		const struct ec_config_schema *schema,
		size_t schema_len)
{
	if (dict->type != EC_CONFIG_TYPE_DICT || schema == NULL) {
		errno = EINVAL;
		goto fail;
	}

	if (ec_config_dict_validate(dict->dict, schema, schema_len) < 0)
		goto fail;

	return 0
;
fail:
	return -1;
}

struct ec_config *
ec_config_get(const struct ec_config *config, const char *key)
{
	if (config == NULL)
		return NULL;

	return ec_keyval_get(config->dict, key);
}

struct ec_config *
ec_config_list_first(struct ec_config *list)
{
	if (list  == NULL || list->type != EC_CONFIG_TYPE_LIST) {
		errno = EINVAL;
		return NULL;
	}

	return TAILQ_FIRST(&list->list);
}

struct ec_config *
ec_config_list_next(struct ec_config *list, struct ec_config *config)
{
	(void)list;
	return TAILQ_NEXT(config, next);
}

/* value is consumed */
int ec_config_dict_set(struct ec_config *config, const char *key,
		struct ec_config *value)
{
	void (*free_cb)(struct ec_config *) = ec_config_free;

	if (config == NULL || key == NULL || value == NULL) {
		errno = EINVAL;
		goto fail;
	}
	if (config->type != EC_CONFIG_TYPE_DICT) {
		errno = EINVAL;
		goto fail;
	}

	return ec_keyval_set(config->dict, key, value,
			(void (*)(void *))free_cb);

fail:
	ec_config_free(value);
	return -1;
}

int ec_config_dict_del(struct ec_config *config, const char *key)
{
	if (config == NULL || key == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (config->type != EC_CONFIG_TYPE_DICT) {
		errno = EINVAL;
		return -1;
	}

	return ec_keyval_del(config->dict, key);
}

/* value is consumed */
int
ec_config_list_add(struct ec_config *list,
		struct ec_config *value)
{
	if (list == NULL || list->type != EC_CONFIG_TYPE_LIST) {
		errno = EINVAL;
		goto fail;
	}

	TAILQ_INSERT_TAIL(&list->list, value, next);

	return 0;

fail:
	ec_config_free(value);
	return -1;
}

int ec_config_list_del(struct ec_config *list, struct ec_config *config)
{
	if (list == NULL || list->type != EC_CONFIG_TYPE_LIST) {
		errno = EINVAL;
		return -1;
	}

	TAILQ_REMOVE(&list->list, config, next);
	ec_config_free(config);
	return 0;
}

static int
ec_config_list_dump(FILE *out, const struct ec_config_list *list,
		size_t indent)
{
	const struct ec_config *v;

	fprintf(out, "%*s" "type=list:\n", (int)indent * 4, "");

	TAILQ_FOREACH(v, list, next) {
		if (__ec_config_dump(out, NULL, v, indent + 1) < 0)
			return -1;
	}

	return 0;
}

static int
ec_config_dict_dump(FILE *out, const struct ec_keyval *dict,
		size_t indent)
{
	const struct ec_config *value;
	struct ec_keyval_iter *iter;
	const char *key;

	fprintf(out, "%*s" "type=dict:\n", (int)indent * 4, "");
	for (iter = ec_keyval_iter(dict);
	     ec_keyval_iter_valid(iter);
	     ec_keyval_iter_next(iter)) {
		key = ec_keyval_iter_get_key(iter);
		value = ec_keyval_iter_get_val(iter);
		if (__ec_config_dump(out, key, value, indent + 1) < 0)
			goto fail;
	}
	ec_keyval_iter_free(iter);
	return 0;

fail:
	ec_keyval_iter_free(iter);
	return -1;
}

static int
__ec_config_dump(FILE *out, const char *key, const struct ec_config *value,
		size_t indent)
{
	char *val_str = NULL;

	switch (value->type) {
	case EC_CONFIG_TYPE_BOOL:
		if (value->boolean)
			ec_asprintf(&val_str, "true");
		else
			ec_asprintf(&val_str, "false");
		break;
	case EC_CONFIG_TYPE_INT64:
		ec_asprintf(&val_str, "%"PRIu64, value->u64);
		break;
	case EC_CONFIG_TYPE_UINT64:
		ec_asprintf(&val_str, "%"PRIi64, value->i64);
		break;
	case EC_CONFIG_TYPE_STRING:
		ec_asprintf(&val_str, "%s", value->string);
		break;
	case EC_CONFIG_TYPE_NODE:
		ec_asprintf(&val_str, "%p", value->node);
		break;
	case EC_CONFIG_TYPE_LIST:
		return ec_config_list_dump(out, &value->list, indent);
	case EC_CONFIG_TYPE_DICT:
		return ec_config_dict_dump(out, value->dict, indent);
	default:
		errno = EINVAL;
		break;
	}

	/* errno is already set on error */
	if (val_str == NULL)
		goto fail;

	fprintf(out, "%*s" "%s%s%stype=%s val=%s\n", (int)indent * 4, "",
		key ? "key=": "",
		key ? key: "",
		key ? " ": "",
		ec_config_type_str(value->type), val_str);

	ec_free(val_str);
	return 0;

fail:
	ec_free(val_str);
	return -1;
}

void
ec_config_dump(FILE *out, const struct ec_config *config)
{
	fprintf(out, "------------------- config dump:\n");

	if (config == NULL) {
		fprintf(out, "no config\n");
		return;
	}

	if (__ec_config_dump(out, NULL, config, 0) < 0)
		fprintf(out, "error while dumping\n");
}

/* LCOV_EXCL_START */
static const struct ec_config_schema sch_intlist_elt[] = {
	{
		.desc = "This is a description for int",
		.type = EC_CONFIG_TYPE_INT64,
	},
};

static const struct ec_config_schema sch_dict[] = {
	{
		.key = "my_int",
		.desc = "This is a description for int",
		.type = EC_CONFIG_TYPE_INT64,
	},
	{
		.key = "my_int2",
		.desc = "This is a description for int2",
		.type = EC_CONFIG_TYPE_INT64,
	},
};

static const struct ec_config_schema sch_dictlist_elt[] = {
	{
		.desc = "This is a description for dict",
		.type = EC_CONFIG_TYPE_DICT,
		.subschema = sch_dict,
		.subschema_len = EC_COUNT_OF(sch_dict),
	},
};

static const struct ec_config_schema sch_baseconfig[] = {
	{
		.key = "my_bool",
		.desc = "This is a description for bool",
		.type = EC_CONFIG_TYPE_BOOL,
	},
	{
		.key = "my_int",
		.desc = "This is a description for int",
		.type = EC_CONFIG_TYPE_INT64,
	},
	{
		.key = "my_string",
		.desc = "This is a description for string",
		.type = EC_CONFIG_TYPE_STRING,
	},
	{
		.key = "my_node",
		.desc = "This is a description for node",
		.type = EC_CONFIG_TYPE_NODE,
	},
	{
		.key = "my_intlist",
		.desc = "This is a description for list",
		.type = EC_CONFIG_TYPE_LIST,
		.subschema = sch_intlist_elt,
		.subschema_len = EC_COUNT_OF(sch_intlist_elt),
	},
	{
		.key = "my_dictlist",
		.desc = "This is a description for list",
		.type = EC_CONFIG_TYPE_LIST,
		.subschema = sch_dictlist_elt,
		.subschema_len = EC_COUNT_OF(sch_dictlist_elt),
	},
};

static int ec_config_testcase(void)
{
	struct ec_node *node = NULL;
	struct ec_keyval *dict = NULL;
	const struct ec_config *value = NULL;
	struct ec_config *config = NULL, *list = NULL, *subconfig = NULL;
	struct ec_config *list_, *config_;
	int testres = 0;
	int ret;

	node = ec_node("empty", EC_NO_ID);
	if (node == NULL)
		goto fail;

	if (ec_config_schema_validate(sch_baseconfig,
					EC_COUNT_OF(sch_baseconfig)) < 0) {
		EC_LOG(EC_LOG_ERR, "invalid config schema\n");
		goto fail;
	}

	ec_config_schema_dump(stdout, sch_baseconfig,
			EC_COUNT_OF(sch_baseconfig));

	config = ec_config_dict();
	if (config == NULL)
		goto fail;

	ret = ec_config_dict_set(config, "my_bool", ec_config_bool(true));
	testres |= EC_TEST_CHECK(ret == 0, "cannot set boolean");
	value = ec_config_get(config, "my_bool");
	testres |= EC_TEST_CHECK(
		value != NULL &&
		value->type == EC_CONFIG_TYPE_BOOL &&
		value->boolean == true,
		"unexpected boolean value");

	ret = ec_config_dict_set(config, "my_int", ec_config_i64(1234));
	testres |= EC_TEST_CHECK(ret == 0, "cannot set int");
	value = ec_config_get(config, "my_int");
	testres |= EC_TEST_CHECK(
		value != NULL &&
		value->type == EC_CONFIG_TYPE_INT64 &&
		value->i64 == 1234,
		"unexpected int value");

	testres |= EC_TEST_CHECK(
		ec_config_validate(config, sch_baseconfig,
				EC_COUNT_OF(sch_baseconfig)) == 0,
		"cannot validate config\n");

	ret = ec_config_dict_set(config, "my_string", ec_config_string("toto"));
	testres |= EC_TEST_CHECK(ret == 0, "cannot set string");
	value = ec_config_get(config, "my_string");
	testres |= EC_TEST_CHECK(
		value != NULL &&
		value->type == EC_CONFIG_TYPE_STRING &&
		!strcmp(value->string, "toto"),
		"unexpected string value");

	list = ec_config_list();
	if (list == NULL)
		goto fail;

	subconfig = ec_config_dict();
	if (subconfig == NULL)
		goto fail;

	ret = ec_config_dict_set(subconfig, "my_int", ec_config_i64(1));
	testres |= EC_TEST_CHECK(ret == 0, "cannot set int");
	value = ec_config_get(subconfig, "my_int");
	testres |= EC_TEST_CHECK(
		value != NULL &&
		value->type == EC_CONFIG_TYPE_INT64 &&
		value->i64 == 1,
		"unexpected int value");

	ret = ec_config_dict_set(subconfig, "my_int2", ec_config_i64(2));
	testres |= EC_TEST_CHECK(ret == 0, "cannot set int");
	value = ec_config_get(subconfig, "my_int2");
	testres |= EC_TEST_CHECK(
		value != NULL &&
		value->type == EC_CONFIG_TYPE_INT64 &&
		value->i64 == 2,
		"unexpected int value");

	testres |= EC_TEST_CHECK(
		ec_config_validate(subconfig, sch_dict,
				EC_COUNT_OF(sch_dict)) == 0,
		"cannot validate subconfig\n");

	ret = ec_config_list_add(list, subconfig);
	subconfig = NULL; /* freed */
	testres |= EC_TEST_CHECK(ret == 0, "cannot add in list");

	subconfig = ec_config_dict();
	if (subconfig == NULL)
		goto fail;

	ret = ec_config_dict_set(subconfig, "my_int", ec_config_i64(3));
	testres |= EC_TEST_CHECK(ret == 0, "cannot set int");
	value = ec_config_get(subconfig, "my_int");
	testres |= EC_TEST_CHECK(
		value != NULL &&
		value->type == EC_CONFIG_TYPE_INT64 &&
		value->i64 == 3,
		"unexpected int value");

	ret = ec_config_dict_set(subconfig, "my_int2", ec_config_i64(4));
	testres |= EC_TEST_CHECK(ret == 0, "cannot set int");
	value = ec_config_get(subconfig, "my_int2");
	testres |= EC_TEST_CHECK(
		value != NULL &&
		value->type == EC_CONFIG_TYPE_INT64 &&
		value->i64 == 4,
		"unexpected int value");

	testres |= EC_TEST_CHECK(
		ec_config_validate(subconfig, sch_dict,
				EC_COUNT_OF(sch_dict)) == 0,
		"cannot validate subconfig\n");

	ret = ec_config_list_add(list, subconfig);
	subconfig = NULL; /* freed */
	testres |= EC_TEST_CHECK(ret == 0, "cannot add in list");

	ret = ec_config_dict_set(config, "my_dictlist", list);
	list = NULL;
	testres |= EC_TEST_CHECK(ret == 0, "cannot set list");

	testres |= EC_TEST_CHECK(
		ec_config_validate(config, sch_baseconfig,
				EC_COUNT_OF(sch_baseconfig)) == 0,
		"cannot validate config\n");

	list_ = ec_config_get(config, "my_dictlist");
	printf("list = %p\n", list_);
	for (config_ = ec_config_list_first(list_); config_ != NULL;
	     config_ = ec_config_list_next(list_, config_)) {
		ec_config_dump(stdout, config_);
	}

	ec_config_dump(stdout, config);

	/* remove the first element */
	ec_config_list_del(list_, ec_config_list_first(list_));
	testres |= EC_TEST_CHECK(
		ec_config_validate(config, sch_baseconfig,
				EC_COUNT_OF(sch_baseconfig)) == 0,
		"cannot validate config\n");

	ec_config_dump(stdout, config);

	ec_config_free(list);
	ec_config_free(subconfig);
	ec_config_free(config);
	ec_keyval_free(dict);
	ec_node_free(node);

	return testres;

fail:
	ec_config_free(list);
	ec_config_free(subconfig);
	ec_config_free(config);
	ec_keyval_free(dict);
	ec_node_free(node);

	return -1;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_config_test = {
	.name = "config",
	.test = ec_config_testcase,
};

EC_TEST_REGISTER(ec_config_test);
