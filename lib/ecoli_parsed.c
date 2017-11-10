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
#include <ecoli_strvec.h>
#include <ecoli_keyval.h>
#include <ecoli_log.h>
#include <ecoli_node.h>
#include <ecoli_parsed.h>

int ec_node_parse_child(struct ec_node *node, struct ec_parsed *state,
			const struct ec_strvec *strvec)
{
	struct ec_strvec *match_strvec;
	struct ec_parsed *child;
	int ret;

	assert(state != NULL);

	/* build the node if required */
	if (node->type->build != NULL) {
		if ((node->flags & EC_NODE_F_BUILT) == 0) {
			ret = node->type->build(node);
			if (ret < 0)
				return ret;
		}
	}
	node->flags |= EC_NODE_F_BUILT;

	if (node->type->parse == NULL)
		return -ENOTSUP;

	child = ec_parsed();
	if (child == NULL)
		return -ENOMEM;

	child->node = node;
	ec_parsed_add_child(state, child);
	ret = node->type->parse(node, child, strvec);
	if (ret == EC_PARSED_NOMATCH) {
		ec_parsed_del_child(state, child);
		assert(TAILQ_EMPTY(&child->children));
		ec_parsed_free(child);
		return ret;
	} else if (ret < 0) {
		return ret;
	}

	match_strvec = ec_strvec_ndup(strvec, 0, ret);
	if (match_strvec == NULL)
		return -ENOMEM;

	child->strvec = match_strvec;

	return ret;
}

struct ec_parsed *ec_node_parse_strvec(struct ec_node *node,
				const struct ec_strvec *strvec)
{
	struct ec_parsed *parsed = ec_parsed();
	struct ec_parsed *child_parsed;
	int ret;

	if (parsed == NULL)
		return NULL;

	ret = ec_node_parse_child(node, parsed, strvec);
	if (ret < 0 && ret != EC_PARSED_NOMATCH) {
		ec_parsed_free(parsed);
		parsed = NULL;
	} else if (ret != EC_PARSED_NOMATCH) {
		/* remove dummy root node */
		child_parsed = ec_parsed_get_last_child(parsed);
		ec_parsed_del_child(parsed, child_parsed);
		ec_parsed_free(parsed);
		parsed = child_parsed;
	}

	return parsed;
}

struct ec_parsed *ec_node_parse(struct ec_node *node, const char *str)
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

struct ec_parsed *ec_parsed(void)
{
	struct ec_parsed *parsed = NULL;

	parsed = ec_calloc(1, sizeof(*parsed));
	if (parsed == NULL)
		goto fail;

	TAILQ_INIT(&parsed->children);

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
	ec_keyval_free(parsed->attrs);
	ec_free(parsed);
}

static void __ec_parsed_dump(FILE *out,
	const struct ec_parsed *parsed, size_t indent)
{
	struct ec_parsed *child;
	const struct ec_strvec *vec;
	size_t i;
	const char *id = "none", *typename = "none";

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

	fprintf(out, "node_type=%s id=%s vec=", typename, id);
	vec = ec_parsed_strvec(parsed);
	ec_strvec_dump(out, vec);

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
	child->parent = parsed;
}

void ec_parsed_del_child(struct ec_parsed *parsed, // XXX rename del in unlink?
	struct ec_parsed *child)
{
	TAILQ_REMOVE(&parsed->children, child, next);
	child->parent = NULL;
}

struct ec_parsed *
ec_parsed_get_last_child(struct ec_parsed *parsed)
{
	return TAILQ_LAST(&parsed->children, ec_parsed_list);
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

const struct ec_strvec *ec_parsed_strvec(const struct ec_parsed *parsed)
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

	if (parsed->node == NULL && TAILQ_EMPTY(&parsed->children)) // XXX both needed?
		return 0;

	return 1;
}
