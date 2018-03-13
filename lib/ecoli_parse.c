/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <ecoli_assert.h>
#include <ecoli_malloc.h>
#include <ecoli_strvec.h>
#include <ecoli_keyval.h>
#include <ecoli_log.h>
#include <ecoli_node.h>
#include <ecoli_parse.h>

TAILQ_HEAD(ec_parse_list, ec_parse);

struct ec_parse {
	TAILQ_ENTRY(ec_parse) next;
	struct ec_parse_list children;
	struct ec_parse *parent;
	const struct ec_node *node;
	struct ec_strvec *strvec;
	struct ec_keyval *attrs;
};

static int __ec_node_parse_child(const struct ec_node *node,
				struct ec_parse *state,
				bool is_root, const struct ec_strvec *strvec)
{
	struct ec_strvec *match_strvec;
	struct ec_parse *child = NULL;
	int ret;

	if (ec_node_type(node)->parse == NULL)
		return -ENOTSUP;

	if (!is_root) {
		child = ec_parse(node);
		if (child == NULL)
			return -ENOMEM;

		ec_parse_link_child(state, child);
	} else {
		child = state;
	}
	ret = ec_node_type(node)->parse(node, child, strvec);
	if (ret < 0 || ret == EC_PARSE_NOMATCH)
		goto free;

	match_strvec = ec_strvec_ndup(strvec, 0, ret);
	if (match_strvec == NULL) {
		ret = -ENOMEM;
		goto free;
	}

	child->strvec = match_strvec;

	return ret;

free:
	if (!is_root) {
		ec_parse_unlink_child(state, child);
		ec_parse_free(child);
	}
	return ret;
}

int ec_node_parse_child(const struct ec_node *node, struct ec_parse *state,
			const struct ec_strvec *strvec)
{
	assert(state != NULL);
	return __ec_node_parse_child(node, state, false, strvec);
}

struct ec_parse *ec_node_parse_strvec(const struct ec_node *node,
				const struct ec_strvec *strvec)
{
	struct ec_parse *parse = ec_parse(node);
	int ret;

	if (parse == NULL)
		return NULL;

	ret = __ec_node_parse_child(node, parse, true, strvec);
	if (ret < 0) {
		ec_parse_free(parse);
		return NULL;
	}

	return parse;
}

struct ec_parse *ec_node_parse(const struct ec_node *node, const char *str)
{
	struct ec_strvec *strvec = NULL;
	struct ec_parse *parse = NULL;

	errno = ENOMEM;
	strvec = ec_strvec();
	if (strvec == NULL)
		goto fail;

	if (ec_strvec_add(strvec, str) < 0)
		goto fail;

	parse = ec_node_parse_strvec(node, strvec);
	if (parse == NULL)
		goto fail;

	ec_strvec_free(strvec);
	return parse;

 fail:
	ec_strvec_free(strvec);
	ec_parse_free(parse);
	return NULL;
}

struct ec_parse *ec_parse(const struct ec_node *node)
{
	struct ec_parse *parse = NULL;

	parse = ec_calloc(1, sizeof(*parse));
	if (parse == NULL)
		goto fail;

	TAILQ_INIT(&parse->children);

	parse->node = node;
	parse->attrs = ec_keyval();
	if (parse->attrs == NULL)
		goto fail;

	return parse;

 fail:
	if (parse != NULL)
		ec_keyval_free(parse->attrs);
	ec_free(parse);

	return NULL;
}

static struct ec_parse *
__ec_parse_dup(const struct ec_parse *root, const struct ec_parse *ref,
		struct ec_parse **new_ref)
{
	struct ec_parse *dup = NULL;
	struct ec_parse *child, *dup_child;
	struct ec_keyval *attrs = NULL;

	if (root == NULL)
		return NULL;

	dup = ec_parse(root->node);
	if (dup == NULL)
		return NULL;

	if (root == ref)
		*new_ref = dup;

	attrs = ec_keyval_dup(root->attrs);
	if (attrs == NULL)
		goto fail;
	ec_keyval_free(dup->attrs);
	dup->attrs = attrs;

	if (root->strvec != NULL) {
		dup->strvec = ec_strvec_dup(root->strvec);
		if (dup->strvec == NULL)
			goto fail;
	}

	TAILQ_FOREACH(child, &root->children, next) {
		dup_child = __ec_parse_dup(child, ref, new_ref);
		if (dup_child == NULL)
			goto fail;
		ec_parse_link_child(dup, dup_child);
	}

	return dup;

fail:
	ec_parse_free(dup);
	return NULL;
}

struct ec_parse *ec_parse_dup(const struct ec_parse *parse)
{
	const struct ec_parse *root;
	struct ec_parse *dup_root, *dup = NULL;

	root = ec_parse_get_root(parse);
	dup_root = __ec_parse_dup(root, parse, &dup);
	if (dup_root == NULL)
		return NULL;
	assert(dup != NULL);

	return dup;
}

void ec_parse_free_children(struct ec_parse *parse)
{
	struct ec_parse *child;

	if (parse == NULL)
		return;

	while (!TAILQ_EMPTY(&parse->children)) {
		child = TAILQ_FIRST(&parse->children);
		TAILQ_REMOVE(&parse->children, child, next);
		child->parent = NULL;
		ec_parse_free(child);
	}
}

void ec_parse_free(struct ec_parse *parse)
{
	if (parse == NULL)
		return;

	ec_assert_print(parse->parent == NULL,
			"parent not NULL in ec_parse_free()");

	ec_parse_free_children(parse);
	ec_strvec_free(parse->strvec);
	ec_keyval_free(parse->attrs);
	ec_free(parse);
}

static void __ec_parse_dump(FILE *out,
	const struct ec_parse *parse, size_t indent)
{
	struct ec_parse *child;
	const struct ec_strvec *vec;
	const char *id, *typename = "none";

	/* node can be null when parsing is incomplete */
	if (parse->node != NULL) {
		id = parse->node->id;
		typename = ec_node_type(parse->node)->name;
	}

	fprintf(out, "%*s" "type=%s id=%s vec=",
		(int)indent * 4, "", typename, id);
	vec = ec_parse_strvec(parse);
	ec_strvec_dump(out, vec);

	TAILQ_FOREACH(child, &parse->children, next)
		__ec_parse_dump(out, child, indent + 1);
}

void ec_parse_dump(FILE *out, const struct ec_parse *parse)
{
	fprintf(out, "------------------- parse dump:\n");

	if (parse == NULL) {
		fprintf(out, "parse is NULL\n");
		return;
	}

	/* only exist if it does not match (strvec == NULL) and if it
	 * does not have children: an incomplete parse, like those
	 * generated by complete() don't match but have children that
	 * may match. */
	if (!ec_parse_matches(parse) && TAILQ_EMPTY(&parse->children)) {
		fprintf(out, "no match\n");
		return;
	}

	__ec_parse_dump(out, parse, 0);
}

void ec_parse_link_child(struct ec_parse *parse,
	struct ec_parse *child)
{
	TAILQ_INSERT_TAIL(&parse->children, child, next);
	child->parent = parse;
}

void ec_parse_unlink_child(struct ec_parse *parse,
	struct ec_parse *child)
{
	TAILQ_REMOVE(&parse->children, child, next);
	child->parent = NULL;
}

struct ec_parse *
ec_parse_get_first_child(const struct ec_parse *parse)
{
	return TAILQ_FIRST(&parse->children);
}

struct ec_parse *
ec_parse_get_last_child(const struct ec_parse *parse)
{
	return TAILQ_LAST(&parse->children, ec_parse_list);
}

struct ec_parse *ec_parse_get_next(const struct ec_parse *parse)
{
	return TAILQ_NEXT(parse, next);
}

bool ec_parse_has_child(const struct ec_parse *parse)
{
	return !TAILQ_EMPTY(&parse->children);
}

const struct ec_node *ec_parse_get_node(const struct ec_parse *parse)
{
	return parse->node;
}

void ec_parse_del_last_child(struct ec_parse *parse)
{
	struct ec_parse *child;

	child = ec_parse_get_last_child(parse);
	ec_parse_unlink_child(parse, child);
	ec_parse_free(child);
}

struct ec_parse *__ec_parse_get_root(struct ec_parse *parse)
{
	if (parse == NULL)
		return NULL;

	while (parse->parent != NULL)
		parse = parse->parent;

	return parse;
}

struct ec_parse *ec_parse_get_parent(const struct ec_parse *parse)
{
	if (parse == NULL)
		return NULL;

	return parse->parent;
}

struct ec_parse *ec_parse_iter_next(struct ec_parse *parse)
{
	struct ec_parse *child, *parent, *next;

	child = TAILQ_FIRST(&parse->children);
	if (child != NULL)
		return child;
	parent = parse->parent;
	while (parent != NULL) {
		next = TAILQ_NEXT(parse, next);
		if (next != NULL)
			return next;
		parse = parent;
		parent = parse->parent;
	}
	return NULL;
}

struct ec_parse *ec_parse_find_first(struct ec_parse *parse,
	const char *id)
{
	struct ec_parse *child, *ret;

	if (parse == NULL)
		return NULL;

	if (parse->node != NULL &&
			parse->node->id != NULL &&
			!strcmp(parse->node->id, id))
		return parse;

	TAILQ_FOREACH(child, &parse->children, next) {
		ret = ec_parse_find_first(child, id);
		if (ret != NULL)
			return ret;
	}

	return NULL;
}

struct ec_keyval *
ec_parse_get_attrs(struct ec_parse *parse)
{
	if (parse == NULL)
		return NULL;

	return parse->attrs;
}

const struct ec_strvec *ec_parse_strvec(const struct ec_parse *parse)
{
	if (parse == NULL || parse->strvec == NULL)
		return NULL;

	return parse->strvec;
}

/* number of strings in the parse vector */
size_t ec_parse_len(const struct ec_parse *parse)
{
	if (parse == NULL || parse->strvec == NULL)
		return 0;

	return ec_strvec_len(parse->strvec);
}

size_t ec_parse_matches(const struct ec_parse *parse)
{
	if (parse == NULL)
		return 0;

	if (parse->strvec == NULL)
		return 0;

	return 1;
}
