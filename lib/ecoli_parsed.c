/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016, Olivier MATZ <zer0@droids-corp.org>
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
#include <ecoli_parsed.h>

TAILQ_HEAD(ec_parsed_list, ec_parsed);

struct ec_parsed {
	TAILQ_ENTRY(ec_parsed) next;
	struct ec_parsed_list children;
	struct ec_parsed *parent;
	const struct ec_node *node;
	struct ec_strvec *strvec;
	struct ec_keyval *attrs;
};

static int __ec_node_parse_child(const struct ec_node *node,
				struct ec_parsed *state,
				bool is_root, const struct ec_strvec *strvec)
{
	struct ec_strvec *match_strvec;
	struct ec_parsed *child = NULL;
	int ret;

	if (node->type->parse == NULL)
		return -ENOTSUP;

	if (!is_root) {
		child = ec_parsed(node);
		if (child == NULL)
			return -ENOMEM;

		ec_parsed_add_child(state, child);
	} else {
		child = state;
	}
	ret = node->type->parse(node, child, strvec);
	if (ret < 0 || ret == EC_PARSED_NOMATCH)
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
		ec_parsed_del_child(state, child);
		ec_parsed_free(child);
	}
	return ret;
}

int ec_node_parse_child(const struct ec_node *node, struct ec_parsed *state,
			const struct ec_strvec *strvec)
{
	assert(state != NULL);
	return __ec_node_parse_child(node, state, false, strvec);
}

struct ec_parsed *ec_node_parse_strvec(const struct ec_node *node,
				const struct ec_strvec *strvec)
{
	struct ec_parsed *parsed = ec_parsed(node);
	int ret;

	if (parsed == NULL)
		return NULL;

	ret = __ec_node_parse_child(node, parsed, true, strvec);
	if (ret < 0) {
		ec_parsed_free(parsed);
		return NULL;
	}

	return parsed;
}

struct ec_parsed *ec_node_parse(const struct ec_node *node, const char *str)
{
	struct ec_strvec *strvec = NULL;
	struct ec_parsed *parsed = NULL;

	errno = ENOMEM;
	strvec = ec_strvec();
	if (strvec == NULL)
		goto fail;

	if (ec_strvec_add(strvec, str) < 0)
		goto fail;

	parsed = ec_node_parse_strvec(node, strvec);
	if (parsed == NULL)
		goto fail;

	ec_strvec_free(strvec);
	return parsed;

 fail:
	ec_strvec_free(strvec);
	ec_parsed_free(parsed);
	return NULL;
}

struct ec_parsed *ec_parsed(const struct ec_node *node)
{
	struct ec_parsed *parsed = NULL;

	parsed = ec_calloc(1, sizeof(*parsed));
	if (parsed == NULL)
		goto fail;

	TAILQ_INIT(&parsed->children);

	parsed->node = node;
	parsed->attrs = ec_keyval();
	if (parsed->attrs == NULL)
		goto fail;

	return parsed;

 fail:
	if (parsed != NULL)
		ec_keyval_free(parsed->attrs);
	ec_free(parsed);

	return NULL;
}

static struct ec_parsed *
__ec_parsed_dup(const struct ec_parsed *root, const struct ec_parsed *ref,
		struct ec_parsed **new_ref)
{
	struct ec_parsed *dup = NULL;
	struct ec_parsed *child, *dup_child;
	struct ec_keyval *attrs = NULL;

	if (root == NULL)
		return NULL;

	dup = ec_parsed(root->node);
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
		dup_child = __ec_parsed_dup(child, ref, new_ref);
		if (dup_child == NULL)
			goto fail;
		ec_parsed_add_child(dup, dup_child);
	}

	return dup;

fail:
	ec_parsed_free(dup);
	return NULL;
}

struct ec_parsed *ec_parsed_dup(struct ec_parsed *parsed) //XXX const
{
	struct ec_parsed *root, *dup_root, *dup = NULL;

	root = ec_parsed_get_root(parsed);
	dup_root = __ec_parsed_dup(root, parsed, &dup);
	if (dup_root == NULL)
		return NULL;
	assert(dup != NULL);

	return dup;
}

void ec_parsed_free_children(struct ec_parsed *parsed)
{
	struct ec_parsed *child;

	if (parsed == NULL)
		return;

	while (!TAILQ_EMPTY(&parsed->children)) {
		child = TAILQ_FIRST(&parsed->children);
		TAILQ_REMOVE(&parsed->children, child, next);
		ec_parsed_free(child);
	}
}

void ec_parsed_free(struct ec_parsed *parsed)
{
	if (parsed == NULL)
		return;

	// assert(parsed->parent == NULL); XXX
	// or
	// parsed = ec_parsed_get_root(parsed);

	ec_parsed_free_children(parsed);
	ec_strvec_free(parsed->strvec);
	ec_keyval_free(parsed->attrs);
	ec_free(parsed);
}

static void __ec_parsed_dump(FILE *out,
	const struct ec_parsed *parsed, size_t indent)
{
	struct ec_parsed *child;
	const struct ec_strvec *vec;
	const char *id, *typename = "none";

	/* node can be null when parsing is incomplete */
	if (parsed->node != NULL) {
		id = parsed->node->id;
		typename = parsed->node->type->name;
	}

	fprintf(out, "%*s" "type=%s id=%s vec=",
		(int)indent * 4, "", typename, id);
	vec = ec_parsed_strvec(parsed);
	ec_strvec_dump(out, vec);

	TAILQ_FOREACH(child, &parsed->children, next)
		__ec_parsed_dump(out, child, indent + 2);
}

void ec_parsed_dump(FILE *out, const struct ec_parsed *parsed)
{
	fprintf(out, "------------------- parsed dump:\n");

	if (parsed == NULL) {
		fprintf(out, "parsed is NULL, error in parse\n");
		return;
	}

	/* only exist if it does not match (strvec == NULL) and if it
	 * does not have children: an incomplete parse, like those
	 * generated by complete() don't match but have children that
	 * may match. */
	if (!ec_parsed_matches(parsed) && TAILQ_EMPTY(&parsed->children)) {
		fprintf(out, "no match\n");
		return;
	}

	__ec_parsed_dump(out, parsed, 0);
}

void ec_parsed_add_child(struct ec_parsed *parsed,
	struct ec_parsed *child)
{
	TAILQ_INSERT_TAIL(&parsed->children, child, next);
	child->parent = parsed;
}

// XXX we can remove the first arg ? parsed == child->parent ?
void ec_parsed_del_child(struct ec_parsed *parsed, // XXX rename del in unlink?
	struct ec_parsed *child)
{
	TAILQ_REMOVE(&parsed->children, child, next);
	child->parent = NULL;
}

struct ec_parsed *
ec_parsed_get_first_child(const struct ec_parsed *parsed)
{
	return TAILQ_FIRST(&parsed->children);
}

struct ec_parsed *
ec_parsed_get_last_child(const struct ec_parsed *parsed)
{
	return TAILQ_LAST(&parsed->children, ec_parsed_list);
}

struct ec_parsed *ec_parsed_get_next(const struct ec_parsed *parsed)
{
	return TAILQ_NEXT(parsed, next);
}

bool ec_parsed_has_child(const struct ec_parsed *parsed)
{
	return !TAILQ_EMPTY(&parsed->children);
}

const struct ec_node *ec_parsed_get_node(const struct ec_parsed *parsed)
{
	return parsed->node;
}

void ec_parsed_del_last_child(struct ec_parsed *parsed) // rename in free
{
	struct ec_parsed *child;

	child = ec_parsed_get_last_child(parsed);
	ec_parsed_del_child(parsed, child);
	ec_parsed_free(child);
}

struct ec_parsed *ec_parsed_get_root(struct ec_parsed *parsed)
{
	if (parsed == NULL)
		return NULL;

	while (parsed->parent != NULL)
		parsed = parsed->parent;

	return parsed;
}

struct ec_parsed *ec_parsed_get_parent(struct ec_parsed *parsed)
{
	if (parsed == NULL)
		return NULL;

	return parsed->parent;
}

struct ec_parsed *ec_parsed_iter_next(struct ec_parsed *parsed)
{
	struct ec_parsed *child, *parent, *next;

	child = TAILQ_FIRST(&parsed->children);
	if (child != NULL)
		return child;
	parent = parsed->parent;
	while (parent != NULL) {
		next = TAILQ_NEXT(parsed, next);
		if (next != NULL)
			return next;
		parsed = parent;
		parent = parsed->parent;
	}
	return NULL;
}

struct ec_parsed *ec_parsed_find_first(struct ec_parsed *parsed,
	const char *id)
{
	struct ec_parsed *child, *ret;

	if (parsed == NULL)
		return NULL;

	if (parsed->node != NULL &&
			parsed->node->id != NULL &&
			!strcmp(parsed->node->id, id))
		return parsed;

	TAILQ_FOREACH(child, &parsed->children, next) {
		ret = ec_parsed_find_first(child, id);
		if (ret != NULL)
			return ret;
	}

	return NULL;
}

struct ec_keyval *
ec_parsed_get_attrs(struct ec_parsed *parsed)
{
	if (parsed == NULL)
		return NULL;

	return parsed->attrs;
}

const struct ec_strvec *ec_parsed_strvec(const struct ec_parsed *parsed)
{
	if (parsed == NULL || parsed->strvec == NULL)
		return NULL;

	return parsed->strvec;
}

/* number of strings in the parsed vector */
size_t ec_parsed_len(const struct ec_parsed *parsed)
{
	if (parsed == NULL || parsed->strvec == NULL)
		return 0;

	return ec_strvec_len(parsed->strvec);
}

size_t ec_parsed_matches(const struct ec_parsed *parsed)
{
	if (parsed == NULL)
		return 0;

	if (parsed->strvec == NULL)
		return 0;

	return 1;
}
