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
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_string.h>
#include <ecoli_strvec.h>
#include <ecoli_keyval.h>
#include <ecoli_log.h>
#include <ecoli_node.h>

EC_LOG_TYPE_REGISTER(node);

static struct ec_node_type_list node_type_list =
	TAILQ_HEAD_INITIALIZER(node_type_list);

struct ec_node_type *ec_node_type_lookup(const char *name)
{
	struct ec_node_type *type;

	TAILQ_FOREACH(type, &node_type_list, next) {
		if (!strcmp(name, type->name))
			return type;
	}

	return NULL;
}

int ec_node_type_register(struct ec_node_type *type)
{
	if (ec_node_type_lookup(type->name) != NULL)
		return -EEXIST;
	if (type->size < sizeof(struct ec_node))
		return -EINVAL;

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
	struct ec_node_type *type;

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
	ec_free(node->attrs);
	ec_free(node);
}

size_t ec_node_get_max_parse_len(const struct ec_node *node)
{
	if (node->type->get_max_parse_len == NULL)
		return SIZE_MAX;
	return node->type->get_max_parse_len(node);
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
	return -1;
}

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

struct ec_keyval *ec_node_attrs(const struct ec_node *node)
{
	return node->attrs;
}

const char *ec_node_id(const struct ec_node *node)
{
	if (node->id == NULL)
		return "None";
	return node->id;
}

static void __ec_node_dump(FILE *out,
	const struct ec_node *node, size_t indent)
{
	const char *id, *typename, *desc;
	struct ec_node *child;
	size_t maxlen;
	size_t i, n;

	maxlen = ec_node_get_max_parse_len(node);
	id = ec_node_id(node);
	typename = node->type->name;
	desc = ec_node_desc(node);

	/* XXX enhance */
	for (i = 0; i < indent; i++) {
		if (i % 2)
			fprintf(out, " ");
		else
			fprintf(out, "|");
	}

	fprintf(out, "node %p type=%s id=%s desc=%s ",
		node, typename, id, desc);
	if (maxlen == SIZE_MAX)
		fprintf(out, "maxlen=no\n");
	else
		fprintf(out, "maxlen=%zu\n", maxlen);
	n = node->n_children;
	for (i = 0; i < n; i++) {
		child = node->children[i];
		__ec_node_dump(out, child, indent + 2);
	}
}

void ec_node_dump(FILE *out, const struct ec_node *node)
{
	fprintf(out, "------------------- node dump:\n"); //XXX

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
