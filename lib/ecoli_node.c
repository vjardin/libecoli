/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_string.h>
#include <ecoli_strvec.h>
#include <ecoli_keyval.h>
#include <ecoli_log.h>
#include <ecoli_config.h>
#include <ecoli_test.h>
#include <ecoli_node.h>

#include <ecoli_node_str.h>
#include <ecoli_node_seq.h>

EC_LOG_TYPE_REGISTER(node);

static struct ec_node_type_list node_type_list =
	TAILQ_HEAD_INITIALIZER(node_type_list);

const struct ec_node_type *
ec_node_type_lookup(const char *name)
{
	struct ec_node_type *type;

	TAILQ_FOREACH(type, &node_type_list, next) {
		if (!strcmp(name, type->name))
			return type;
	}

	errno = ENOENT;
	return NULL;
}

int ec_node_type_register(struct ec_node_type *type)
{
	EC_CHECK_ARG(type->size >= sizeof(struct ec_node), -1, EINVAL);

	if (ec_node_type_lookup(type->name) != NULL) {
		errno = EEXIST;
		return -1;
	}

	TAILQ_INSERT_TAIL(&node_type_list, type, next);

	return 0;
}

void ec_node_type_dump(FILE *out)
{
	struct ec_node_type *type;

	TAILQ_FOREACH(type, &node_type_list, next)
		fprintf(out, "%s\n", type->name);
}

struct ec_node *__ec_node(const struct ec_node_type *type, const char *id)
{
	struct ec_node *node = NULL;

	EC_LOG(EC_LOG_DEBUG, "create node type=%s id=%s\n",
		type->name, id);
	if (id == NULL) {
		errno = EINVAL;
		goto fail;
	}

	node = ec_calloc(1, type->size);
	if (node == NULL)
		goto fail;

	node->type = type;
	node->refcnt = 1;

	node->id = ec_strdup(id);
	if (node->id == NULL)
		goto fail;

	if (ec_asprintf(&node->desc, "<%s>", type->name) < 0)
		goto fail;

	node->attrs = ec_keyval();
	if (node->attrs == NULL)
		goto fail;

	if (type->init_priv != NULL) {
		if (type->init_priv(node) < 0)
			goto fail;
	}

	return node;

 fail:
	if (node != NULL) {
		ec_keyval_free(node->attrs);
		ec_free(node->desc);
		ec_free(node->id);
	}
	ec_free(node);

	return NULL;
}

struct ec_node *ec_node(const char *typename, const char *id)
{
	const struct ec_node_type *type;

	type = ec_node_type_lookup(typename);
	if (type == NULL) {
		EC_LOG(EC_LOG_ERR, "type=%s does not exist\n",
			typename);
		return NULL;
	}

	return __ec_node(type, id);
}

void ec_node_free(struct ec_node *node)
{
	if (node == NULL)
		return;

	assert(node->refcnt > 0);

	if (--node->refcnt > 0)
		return;

	if (node->type != NULL && node->type->free_priv != NULL)
		node->type->free_priv(node);
	ec_free(node->children);
	ec_free(node->id);
	ec_free(node->desc);
	ec_keyval_free(node->attrs);
	ec_config_free(node->config);
	ec_free(node);
}

struct ec_node *ec_node_clone(struct ec_node *node)
{
	if (node != NULL)
		node->refcnt++;
	return node;
}

size_t ec_node_get_children_count(const struct ec_node *node)
{
	return node->n_children;
}

struct ec_node *
ec_node_get_child(const struct ec_node *node, size_t i)
{
	if (i >= ec_node_get_children_count(node))
		return NULL;
	return node->children[i];
}

int ec_node_add_child(struct ec_node *node, struct ec_node *child)
{
	struct ec_node **children = NULL;
	size_t n;

	if (node == NULL || child == NULL) {
		errno = EINVAL;
		goto fail;
	}

	n = node->n_children;
	children = ec_realloc(node->children,
			(n + 1) * sizeof(child));
	if (children == NULL)
		goto fail;

	children[n] = child;
	node->children = children;
	node->n_children = n + 1;

	return 0;

fail:
	ec_free(children);
	assert(errno != 0);
	return -1;
}

int
ec_node_set_config(struct ec_node *node, struct ec_config *config)
{
	if (node->type->schema == NULL) {
		errno = EINVAL;
		goto fail;
	}
	if (ec_config_validate(config, node->type->schema,
				node->type->schema_len) < 0)
		goto fail;
	if (node->type->set_config != NULL) {
		if (node->type->set_config(node, config) < 0)
			goto fail;
	}

	ec_config_free(node->config);
	node->config = config;

	return 0;

fail:
	return -1;
}

const struct ec_config *ec_node_get_config(struct ec_node *node)
{
	return node->config;
}

#if 0 /* later */
int ec_node_del_child(struct ec_node *node, struct ec_node *child)
{
	size_t i, n;

	if (node == NULL || child == NULL)
		goto fail;

	n = node->n_children;
	for (i = 0; i < n; i++) {
		if (node->children[i] != child)
			continue;
		memcpy(&node->children[i], &node->children[i+1],
			(n - i - 1) * sizeof(child));
		return 0;
	}

fail:
	errno = EINVAL;
	return -1;
}
#endif

struct ec_node *ec_node_find(struct ec_node *node, const char *id)
{
	struct ec_node *child, *ret;
	const char *node_id = ec_node_id(node);
	size_t i, n;

	if (id != NULL && node_id != NULL && !strcmp(node_id, id))
		return node;

	n = node->n_children;
	for (i = 0; i < n; i++) {
		child = node->children[i];
		ret = ec_node_find(child, id);
		if (ret != NULL)
			return ret;
	}

	return NULL;
}

const struct ec_node_type *ec_node_type(const struct ec_node *node)
{
	return node->type;
}

struct ec_keyval *ec_node_attrs(const struct ec_node *node)
{
	return node->attrs;
}

const char *ec_node_id(const struct ec_node *node)
{
	return node->id;
}

static void __ec_node_dump(FILE *out,
	const struct ec_node *node, size_t indent)
{
	const char *id, *typename;
	struct ec_node *child;
	size_t i, n;

	id = ec_node_id(node);
	typename = node->type->name;

	fprintf(out, "%*s" "type=%s id=%s %p\n",
		(int)indent * 4, "", typename, id, node);
	n = node->n_children;
	for (i = 0; i < n; i++) {
		child = node->children[i];
		__ec_node_dump(out, child, indent + 1);
	}
}

void ec_node_dump(FILE *out, const struct ec_node *node)
{
	fprintf(out, "------------------- node dump:\n");

	if (node == NULL) {
		fprintf(out, "node is NULL\n");
		return;
	}

	__ec_node_dump(out, node, 0);
}

const char *ec_node_desc(const struct ec_node *node)
{
	if (node->type->desc != NULL)
		return node->type->desc(node);

	return node->desc;
}

int ec_node_check_type(const struct ec_node *node,
		const struct ec_node_type *type)
{
	if (strcmp(node->type->name, type->name)) {
		errno = EINVAL;
		return -1;
	}

	return 0;
}

/* LCOV_EXCL_START */
static int ec_node_testcase(void)
{
	struct ec_node *node = NULL;
	const struct ec_node *child;
	const struct ec_node_type *type;
	FILE *f = NULL;
	char *buf = NULL;
	size_t buflen = 0;
	int testres = 0;
	int ret;

	node = EC_NODE_SEQ(EC_NO_ID,
			ec_node_str("id_x", "x"),
			ec_node_str("id_y", "y"));
	if (node == NULL)
		goto fail;

	ec_node_clone(node);
	ec_node_free(node);

	f = open_memstream(&buf, &buflen);
	if (f == NULL)
		goto fail;
	ec_node_dump(f, node);
	ec_node_type_dump(f);
	ec_node_dump(f, NULL);
	fclose(f);
	f = NULL;

	testres |= EC_TEST_CHECK(
		strstr(buf, "type=seq id=no-id"), "bad dump\n");
	testres |= EC_TEST_CHECK(
		strstr(buf, "type=str id=id_x") &&
		strstr(strstr(buf, "type=str id=id_x") + 1,
			"type=str id=id_y"),
		"bad dump\n");
	free(buf);
	buf = NULL;

	testres |= EC_TEST_CHECK(
		!strcmp(ec_node_type(node)->name, "seq") &&
		!strcmp(ec_node_id(node), EC_NO_ID) &&
		!strcmp(ec_node_desc(node), "<seq>"),
		"bad child 0");

	testres |= EC_TEST_CHECK(
		ec_node_get_children_count(node) == 2,
		"bad children count\n");
	child = ec_node_get_child(node, 0);
	testres |= EC_TEST_CHECK(child != NULL &&
		!strcmp(ec_node_type(child)->name, "str") &&
		!strcmp(ec_node_id(child), "id_x"),
		"bad child 0");
	child = ec_node_get_child(node, 1);
	testres |= EC_TEST_CHECK(child != NULL &&
		!strcmp(ec_node_type(child)->name, "str") &&
		!strcmp(ec_node_id(child), "id_y"),
		"bad child 1");
	child = ec_node_get_child(node, 2);
	testres |= EC_TEST_CHECK(child == NULL,
		"child 2 should be NULL");

	child = ec_node_find(node, "id_x");
	testres |= EC_TEST_CHECK(child != NULL &&
		!strcmp(ec_node_type(child)->name, "str") &&
		!strcmp(ec_node_id(child), "id_x") &&
		!strcmp(ec_node_desc(child), "x"),
		"bad child id_x");
	child = ec_node_find(node, "id_dezdex");
	testres |= EC_TEST_CHECK(child == NULL,
		"child with wrong id should be NULL");

	ret = ec_keyval_set(ec_node_attrs(node), "key", "val", NULL);
	testres |= EC_TEST_CHECK(ret == 0,
		"cannot set node attribute\n");

	type = ec_node_type_lookup("seq");
	testres |= EC_TEST_CHECK(type != NULL &&
		ec_node_check_type(node, type) == 0,
		"cannot get seq node type");
	type = ec_node_type_lookup("str");
	testres |= EC_TEST_CHECK(type != NULL &&
		ec_node_check_type(node, type) < 0,
		"node type should not be str");

	ec_node_free(node);
	node = NULL;

	node = ec_node("deznuindez", EC_NO_ID);
	testres |= EC_TEST_CHECK(node == NULL,
			"should not be able to create node\n");

	return testres;

fail:
	ec_node_free(node);
	if (f != NULL)
		fclose(f);
	free(buf);

	assert(errno != 0);
	return -1;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_test = {
	.name = "node",
	.test = ec_node_testcase,
};

EC_TEST_REGISTER(ec_node_test);
