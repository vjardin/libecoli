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
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_strvec.h>
#include <ecoli_keyval.h>
#include <ecoli_log.h>
#include <ecoli_node.h>

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
	char buf[256]; // XXX

	ec_log(EC_LOG_DEBUG, "create node type=%s id=%s\n", type->name, id);

	node = ec_calloc(1, type->size);
	if (node == NULL)
		goto fail;

	TAILQ_INIT(&node->children);
	node->type = type;
	node->refcnt = 1;

	if (id != NULL) {
		node->id = ec_strdup(id);
		if (node->id == NULL)
			goto fail;
	}

	snprintf(buf, sizeof(buf), "<%s>", type->name);
	node->desc = ec_strdup(buf); // XXX ec_asprintf ?
	if (node->desc == NULL)
		goto fail;

	node->attrs = ec_keyval();
	if (node->attrs == NULL)
		goto fail;

	return node;

 fail:
	ec_node_free(node);
	return NULL;
}

struct ec_node *ec_node(const char *typename, const char *id)
{
	struct ec_node_type *type;

	type = ec_node_type_lookup(typename);
	if (type == NULL) {
		ec_log(EC_LOG_ERR, "type=%s does not exist\n", typename);
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
	ec_free(node->id);
	ec_free(node->desc);
	ec_free(node->attrs);
	ec_free(node);
}

struct ec_node *ec_node_clone(struct ec_node *node)
{
	if (node != NULL)
		node->refcnt++;
	return node;
}

struct ec_node *ec_node_find(struct ec_node *node, const char *id)
{
	struct ec_node *child, *ret;
	const char *node_id = ec_node_id(node);

	if (id != NULL && node_id != NULL && !strcmp(node_id, id))
		return node;

	TAILQ_FOREACH(child, &node->children, next) {
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
	return node->id;
}

struct ec_node *ec_node_parent(const struct ec_node *node)
{
	return node->parent;
}

static void __ec_node_dump(FILE *out,
	const struct ec_node *node, size_t indent)
{
	struct ec_node *child;
	size_t i;
	const char *id = "None", *typename = "None";

	if (node->id != NULL)
		id = node->id;
	typename = node->type->name;

	/* XXX enhance */
	for (i = 0; i < indent; i++) {
		if (i % 2)
			fprintf(out, " ");
		else
			fprintf(out, "|");
	}

	fprintf(out, "node_type=%s id=%s\n", typename, id);
	TAILQ_FOREACH(child, &node->children, next)
		__ec_node_dump(out, child, indent + 2);
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

struct ec_parsed *ec_node_parse(struct ec_node *node, const char *str)
{
	struct ec_strvec *strvec = NULL;
	struct ec_parsed *parsed;

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
	return NULL;
}

struct ec_parsed *ec_node_parse_strvec(struct ec_node *node,
	const struct ec_strvec *strvec)
{
	struct ec_parsed *parsed;
	int ret;

	/* build the node if required */
	if (node->type->build != NULL) {
		if ((node->flags & EC_NODE_F_BUILT) == 0) {
			ret = node->type->build(node);
			if (ret < 0) {
				errno = -ret;
				return NULL;
			}
		}
	}
	node->flags |= EC_NODE_F_BUILT;

	if (node->type->parse == NULL) {
		errno = ENOTSUP;
		return NULL;
	}

	parsed = node->type->parse(node, strvec);

	return parsed;
}


struct ec_parsed *ec_parsed(void)
{
	struct ec_parsed *parsed = NULL;

	parsed = ec_calloc(1, sizeof(*parsed));
	if (parsed == NULL)
		goto fail;

	TAILQ_INIT(&parsed->children);

	return parsed;

 fail:
	return NULL;
}

void ec_parsed_set_match(struct ec_parsed *parsed,
	const struct ec_node *node, struct ec_strvec *strvec)
{
	parsed->node = node;
	parsed->strvec = strvec;
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

	ec_parsed_free_children(parsed);
	ec_strvec_free(parsed->strvec);
	ec_free(parsed);
}

static void __ec_parsed_dump(FILE *out,
	const struct ec_parsed *parsed, size_t indent)
{
	struct ec_parsed *child;
	const struct ec_strvec *vec;
	size_t i;
	const char *id = "None", *typename = "None";

	if (parsed->node != NULL) {
		if (parsed->node->id != NULL)
			id = parsed->node->id;
		typename = parsed->node->type->name;
	}

	/* XXX enhance */
	for (i = 0; i < indent; i++) {
		if (i % 2)
			fprintf(out, " ");
		else
			fprintf(out, "|");
	}

	fprintf(out, "node_type=%s id=%s vec=[", typename, id);
	vec = ec_parsed_strvec(parsed);
	for (i = 0; i < ec_strvec_len(vec); i++)
		fprintf(out, "%s<%s>",
			i == 0 ? "" : ",",
			ec_strvec_val(vec, i));
	fprintf(out, "]\n");

	TAILQ_FOREACH(child, &parsed->children, next)
		__ec_parsed_dump(out, child, indent + 2);
}

void ec_parsed_dump(FILE *out, const struct ec_parsed *parsed)
{
	fprintf(out, "------------------- parsed dump:\n"); //XXX

	if (parsed == NULL) {
		fprintf(out, "parsed is NULL, error in parse\n");
		return;
	}
	if (!ec_parsed_matches(parsed)) {
		fprintf(out, "no match\n");
		return;
	}

	__ec_parsed_dump(out, parsed, 0);
}

void ec_parsed_add_child(struct ec_parsed *parsed,
	struct ec_parsed *child)
{
	TAILQ_INSERT_TAIL(&parsed->children, child, next);
}

void ec_parsed_del_child(struct ec_parsed *parsed,
	struct ec_parsed *child)
{
	TAILQ_REMOVE(&parsed->children, child, next);
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

const struct ec_strvec *ec_parsed_strvec(
	const struct ec_parsed *parsed)
{
	if (parsed == NULL || parsed->strvec == NULL)
		return NULL;

	return parsed->strvec;
}

/* number of parsed strings in the vector */
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

	if (parsed->node == NULL && TAILQ_EMPTY(&parsed->children))
		return 0;

	return 1;
}

struct ec_completed *ec_completed(void)
{
	struct ec_completed *completed = NULL;

	completed = ec_calloc(1, sizeof(*completed));
	if (completed == NULL)
		return NULL;

	TAILQ_INIT(&completed->elts);
	completed->count_match = 0;

	return completed;
}

struct ec_completed_elt *ec_completed_elt(const struct ec_node *node,
	const char *add)
{
	struct ec_completed_elt *elt = NULL;

	elt = ec_calloc(1, sizeof(*elt));
	if (elt == NULL)
		return NULL;

	elt->node = node;
	if (add != NULL) {
		elt->add = ec_strdup(add);
		if (elt->add == NULL) {
			ec_completed_elt_free(elt);
			return NULL;
		}
	}

	return elt;
}

/* XXX define when to use ec_node_complete() or node->complete()
 * (same for parse)
 * suggestion: node->op() is internal, user calls the function
 * other idea: have 2 functions
 */
struct ec_completed *ec_node_complete(struct ec_node *node,
	const char *str)
{
	struct ec_strvec *strvec = NULL;
	struct ec_completed *completed;

	errno = ENOMEM;
	strvec = ec_strvec();
	if (strvec == NULL)
		goto fail;

	if (ec_strvec_add(strvec, str) < 0)
		goto fail;

	completed = ec_node_complete_strvec(node, strvec);
	if (completed == NULL)
		goto fail;

	ec_strvec_free(strvec);
	return completed;

 fail:
	ec_strvec_free(strvec);
	return NULL;
}

/* default completion function: return a no-match element */
struct ec_completed *ec_node_default_complete(const struct ec_node *gen_node,
	const struct ec_strvec *strvec)
{
	struct ec_completed *completed;
	struct ec_completed_elt *completed_elt;

	(void)strvec;

	completed = ec_completed();
	if (completed == NULL)
		return NULL;

	completed_elt = ec_completed_elt(gen_node, NULL);
	if (completed_elt == NULL) {
		ec_completed_free(completed);
		return NULL;
	}

	ec_completed_add_elt(completed, completed_elt);

	return completed;
}

struct ec_completed *ec_node_complete_strvec(struct ec_node *node,
	const struct ec_strvec *strvec)
{
	int ret;

	/* build the node if required */
	if (node->type->build != NULL) {
		if ((node->flags & EC_NODE_F_BUILT) == 0) {
			ret = node->type->build(node);
			if (ret < 0) {
				errno = -ret;
				return NULL;
			}
		}
	}
	node->flags |= EC_NODE_F_BUILT;

	if (node->type->complete == NULL) {
		errno = ENOTSUP;
		return NULL;
	}

	return node->type->complete(node, strvec);
}

/* count the number of identical chars at the beginning of 2 strings */
static size_t strcmp_count(const char *s1, const char *s2)
{
	size_t i = 0;

	while (s1[i] && s2[i] && s1[i] == s2[i])
		i++;

	return i;
}

void ec_completed_add_elt(
	struct ec_completed *completed, struct ec_completed_elt *elt)
{
	size_t n;

	TAILQ_INSERT_TAIL(&completed->elts, elt, next);
	completed->count++;
	if (elt->add != NULL)
		completed->count_match++;
	if (elt->add != NULL) {
		if (completed->smallest_start == NULL) {
			completed->smallest_start = ec_strdup(elt->add);
		} else {
			n = strcmp_count(elt->add,
				completed->smallest_start);
			completed->smallest_start[n] = '\0';
		}
	}
}

void ec_completed_elt_free(struct ec_completed_elt *elt)
{
	ec_free(elt->add);
	ec_free(elt);
}

void ec_completed_merge(struct ec_completed *completed1,
	struct ec_completed *completed2)
{
	struct ec_completed_elt *elt;

	assert(completed1 != NULL);
	assert(completed2 != NULL);

	while (!TAILQ_EMPTY(&completed2->elts)) {
		elt = TAILQ_FIRST(&completed2->elts);
		TAILQ_REMOVE(&completed2->elts, elt, next);
		ec_completed_add_elt(completed1, elt);
	}

	ec_completed_free(completed2);
}

void ec_completed_free(struct ec_completed *completed)
{
	struct ec_completed_elt *elt;

	if (completed == NULL)
		return;

	while (!TAILQ_EMPTY(&completed->elts)) {
		elt = TAILQ_FIRST(&completed->elts);
		TAILQ_REMOVE(&completed->elts, elt, next);
		ec_completed_elt_free(elt);
	}
	ec_free(completed->smallest_start);
	ec_free(completed);
}

void ec_completed_dump(FILE *out, const struct ec_completed *completed)
{
	struct ec_completed_elt *elt;

	if (completed == NULL || completed->count == 0) {
		fprintf(out, "no completion\n");
		return;
	}

	fprintf(out, "completion: count=%u match=%u smallest_start=<%s>\n",
		completed->count, completed->count_match,
		completed->smallest_start);

	TAILQ_FOREACH(elt, &completed->elts, next) {
		fprintf(out, "add=<%s>, node=%p, node_type=%s\n",
			elt->add, elt->node, elt->node->type->name);
	}
}

const char *ec_completed_smallest_start(
	const struct ec_completed *completed)
{
	if (completed == NULL || completed->smallest_start == NULL)
		return "";

	return completed->smallest_start;
}

unsigned int ec_completed_count(
	const struct ec_completed *completed,
	enum ec_completed_filter_flags flags)
{
	unsigned int count = 0;

	if (completed == NULL)
		return count;

	if (flags & EC_MATCH)
		count += completed->count_match;
	if (flags & EC_NO_MATCH)
		count += (completed->count - completed->count_match); //XXX

	return count;
}

struct ec_completed_iter *
ec_completed_iter(struct ec_completed *completed,
	enum ec_completed_filter_flags flags)
{
	struct ec_completed_iter *iter;

	iter = ec_calloc(1, sizeof(*iter));
	if (iter == NULL)
		return NULL;

	iter->completed = completed;
	iter->flags = flags;
	iter->cur = NULL;

	return iter;
}

const struct ec_completed_elt *ec_completed_iter_next(
	struct ec_completed_iter *iter)
{
	if (iter->completed == NULL)
		return NULL;

	do {
		if (iter->cur == NULL) {
			iter->cur = TAILQ_FIRST(&iter->completed->elts);
		} else {
			iter->cur = TAILQ_NEXT(iter->cur, next);
		}

		if (iter->cur == NULL)
			break;

		if (iter->cur->add == NULL &&
				(iter->flags & EC_NO_MATCH))
			break;

		if (iter->cur->add != NULL &&
				(iter->flags & EC_MATCH))
			break;

	} while (iter->cur != NULL);

	return iter->cur;
}

void ec_completed_iter_free(struct ec_completed_iter *iter)
{
	ec_free(iter);
}

const char *ec_node_desc(const struct ec_node *node)
{
	if (node->type->desc != NULL)
		return node->type->desc(node);

	return node->desc;
}
