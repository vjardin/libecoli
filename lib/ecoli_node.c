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
#include <ecoli_node_or.h>
#include <ecoli_node_int.h>

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

static void count_references(struct ec_node *node, unsigned int refs)
{
	struct ec_node *child;
	size_t i, n;

	if (node->free.state == EC_NODE_FREE_STATE_TRAVERSED) {
		node->free.refcnt += refs;
		return;
	}
	node->free.refcnt = refs;
	node->free.state = EC_NODE_FREE_STATE_TRAVERSED;
	n = ec_node_get_children_count(node);
	for (i = 0; i < n; i++) {
		child = ec_node_get_child(node, i);
		refs = ec_node_get_child_refs(node, i);
		count_references(child, refs);
	}
}

static void mark_freeable(struct ec_node *node, enum ec_node_free_state mark)
{
	struct ec_node *child;
	size_t i, n;

	if (mark == node->free.state)
		return;

	if (node->refcnt > node->free.refcnt)
		mark = EC_NODE_FREE_STATE_NOT_FREEABLE;
	assert(node->refcnt >= node->free.refcnt);
	node->free.state = mark;

	n = ec_node_get_children_count(node);
	for (i = 0; i < n; i++) {
		child = ec_node_get_child(node, i);
		mark_freeable(child, mark);
	}
}

static void reset_mark(struct ec_node *node)
{
	struct ec_node *child;
	size_t i, n;

	if (node->free.state == EC_NODE_FREE_STATE_NONE)
		return;

	node->free.state = EC_NODE_FREE_STATE_NONE;
	node->free.refcnt = 0;

	n = ec_node_get_children_count(node);
	for (i = 0; i < n; i++) {
		child = ec_node_get_child(node, i);
		reset_mark(child);
	}
}

/* free a node, taking care of loops in the node graph */
void ec_node_free(struct ec_node *node)
{
	size_t n;

	if (node == NULL)
		return;

	assert(node->refcnt > 0);

	if (node->free.state == EC_NODE_FREE_STATE_NONE &&
			node->refcnt != 1) {

		/* Traverse the node tree starting from this node, and for each
		 * node, count the number of reachable references. Then, all
		 * nodes whose reachable references == total reference are
		 * marked as freeable, and other are marked as unfreeable. Any
		 * node reachable from an unfreeable node is also marked as
		 * unfreeable. */
		if (node->free.state == EC_NODE_FREE_STATE_NONE) {
			count_references(node, 1);
			mark_freeable(node, EC_NODE_FREE_STATE_FREEABLE);
		}
	}

	if (node->free.state == EC_NODE_FREE_STATE_NOT_FREEABLE) {
		node->refcnt--;
		reset_mark(node);
		return;
	}

	if (node->free.state != EC_NODE_FREE_STATE_FREEING) {
		node->free.state = EC_NODE_FREE_STATE_FREEING;
		n = ec_node_get_children_count(node);
		/* children should be freed by free_priv() */
		assert(n == 0 || node->type->free_priv != NULL);
		if (node->type->free_priv != NULL)
			node->type->free_priv(node);
	}

	node->refcnt--;
	if (node->refcnt != 0)
		return;

	node->free.state = EC_NODE_FREE_STATE_NONE;
	node->free.refcnt = 0;

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
	if (node->type->get_children_count == NULL)
		return 0;
	return node->type->get_children_count(node);
}

struct ec_node *
ec_node_get_child(const struct ec_node *node, size_t i)
{
	if (node->type->get_child == NULL)
		return NULL;
	return node->type->get_child(node, i);
}

unsigned int
ec_node_get_child_refs(const struct ec_node *node, size_t i)
{
	if (node->type->get_child_refs == NULL)
		return 1;
	return node->type->get_child_refs(node, i);
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
	ec_config_free(config);
	return -1;
}

const struct ec_config *ec_node_get_config(struct ec_node *node)
{
	return node->config;
}

struct ec_node *ec_node_find(struct ec_node *node, const char *id)
{
	struct ec_node *child, *ret;
	const char *node_id = ec_node_id(node);
	size_t i, n;

	if (id != NULL && node_id != NULL && !strcmp(node_id, id))
		return node;

	n = ec_node_get_children_count(node);
	for (i = 0; i < n; i++) {
		child = ec_node_get_child(node, i);
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
	const struct ec_node *node, size_t indent, struct ec_keyval *dict)
{
	const char *id, *typename;
	struct ec_node *child;
	char buf[32];
	size_t i, n;

	id = ec_node_id(node);
	typename = node->type->name;

	snprintf(buf, sizeof(buf), "%p", node);
	if (ec_keyval_has_key(dict, buf)) {
		fprintf(out, "%*s" "type=%s id=%s %p... (loop)\n",
			(int)indent * 4, "", typename, id, node);
		return;
	}

	ec_keyval_set(dict, buf, NULL, NULL);
	fprintf(out, "%*s" "type=%s id=%s %p refs=%u free_state=%d free_refs=%d\n",
		(int)indent * 4, "", typename, id, node, node->refcnt,
		node->free.state, node->free.refcnt);

	n = ec_node_get_children_count(node);
	for (i = 0; i < n; i++) {
		child = ec_node_get_child(node, i);
		__ec_node_dump(out, child, indent + 1, dict);
	}
}

void ec_node_dump(FILE *out, const struct ec_node *node)
{
	struct ec_keyval *dict = NULL;

	fprintf(out, "------------------- node dump:\n");

	if (node == NULL) {
		fprintf(out, "node is NULL\n");
		return;
	}

	dict = ec_keyval();
	if (dict == NULL)
		goto fail;

	__ec_node_dump(out, node, 0, dict);

	ec_keyval_free(dict);
	return;

fail:
	ec_keyval_free(dict);
	EC_LOG(EC_LOG_ERR, "failed to dump node\n");
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
	struct ec_node *node = NULL, *expr = NULL;
	struct ec_node *expr2 = NULL, *val = NULL, *op = NULL, *seq = NULL;
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

	/* test loop */
	expr = ec_node("or", EC_NO_ID);
	val = ec_node_int(EC_NO_ID, 0, 10, 0);
	op = ec_node_str(EC_NO_ID, "!");
	seq = EC_NODE_SEQ(EC_NO_ID,
			op,
			ec_node_clone(expr));
	op = NULL;
	if (expr == NULL || val == NULL || seq == NULL)
		goto fail;
	if (ec_node_or_add(expr, ec_node_clone(seq)) < 0)
		goto fail;
	ec_node_free(seq);
	seq = NULL;
	if (ec_node_or_add(expr, ec_node_clone(val)) < 0)
		goto fail;
	ec_node_free(val);
	val = NULL;

	testres |= EC_TEST_CHECK_PARSE(expr, 1, "1");
	testres |= EC_TEST_CHECK_PARSE(expr, 3, "!", "!", "1");
	testres |= EC_TEST_CHECK_PARSE(expr, -1, "!", "!", "!");

	ec_node_free(expr);
	expr = NULL;

	/* same loop test, but keep some refs (released later) */
	expr = ec_node("or", EC_NO_ID);
	ec_node_clone(expr);
	expr2 = expr;
	val = ec_node_int(EC_NO_ID, 0, 10, 0);
	op = ec_node_str(EC_NO_ID, "!");
	seq = EC_NODE_SEQ(EC_NO_ID,
			op,
			ec_node_clone(expr));
	op = NULL;
	if (expr == NULL || val == NULL || seq == NULL)
		goto fail;
	if (ec_node_or_add(expr, ec_node_clone(seq)) < 0)
		goto fail;
	ec_node_free(seq);
	seq = NULL;
	if (ec_node_or_add(expr, ec_node_clone(val)) < 0)
		goto fail;

	testres |= EC_TEST_CHECK_PARSE(expr, 1, "1");
	testres |= EC_TEST_CHECK_PARSE(expr, 3, "!", "!", "1");
	testres |= EC_TEST_CHECK_PARSE(expr, -1, "!", "!", "!");

	ec_node_free(expr2);
	expr2 = NULL;
	ec_node_free(val);
	val = NULL;
	ec_node_free(expr);
	expr = NULL;

	return testres;

fail:
	ec_node_free(expr);
	ec_node_free(expr2);
	ec_node_free(val);
	ec_node_free(seq);
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
