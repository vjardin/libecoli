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
#include <ecoli_parsed.h>
#include <ecoli_completed.h>

struct ec_completed *ec_completed(void)
{
	struct ec_completed *completed = NULL;

	completed = ec_calloc(1, sizeof(*completed));
	if (completed == NULL)
		return NULL;

	TAILQ_INIT(&completed->match_items);
	TAILQ_INIT(&completed->no_match_items);

	return completed;
}

struct ec_completed *
ec_node_complete_child(struct ec_node *node,
		struct ec_parsed *state,
		const struct ec_strvec *strvec)
{
	struct ec_completed *completed;
	struct ec_parsed *child;
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

	child = ec_parsed();
	if (child == NULL)
		return NULL;

	child->node = node;
	ec_parsed_add_child(state, child);
	completed = node->type->complete(node, child, strvec);

#if 0 // XXX dump
	printf("----------------------------------------------------------\n");
	ec_node_dump(stdout, node);
	ec_strvec_dump(stdout, strvec);
	ec_completed_dump(stdout, completed);
	ec_parsed_dump(stdout, state);
#endif

	ec_parsed_del_child(state, child);
	assert(TAILQ_EMPTY(&child->children));
	ec_parsed_free(child);

	return completed;
}

struct ec_completed *ec_node_complete_strvec(struct ec_node *node,
	const struct ec_strvec *strvec)
{
	struct ec_parsed *state = ec_parsed();
	struct ec_completed *completed;

	if (state == NULL)
		return NULL;

	completed = ec_node_complete_child(node, state, strvec);
	ec_parsed_free(state);

	return completed;
}

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

/* count the number of identical chars at the beginning of 2 strings */
static size_t strcmp_count(const char *s1, const char *s2)
{
	size_t i = 0;

	while (s1[i] && s2[i] && s1[i] == s2[i])
		i++;

	return i;
}

static struct ec_completed_item *
ec_completed_item(enum ec_completed_type type, struct ec_parsed *state,
		const struct ec_node *node, const char *add)
{
	struct ec_completed_item *item = NULL;

	item = ec_calloc(1, sizeof(*item));
	if (item == NULL)
		return NULL;

	/* XXX can state be NULL? */
	if (state != NULL) {
		struct ec_parsed *p;
		size_t len;

		/* get path len */
		for (p = state, len = 0; p != NULL;
		     p = ec_parsed_get_parent(p), len++)
			;

		item->path = ec_calloc(len, sizeof(*item->path));
		if (item->path == NULL)
			goto fail;

		item->pathlen = len;

		/* write path in array */
		for (p = state, len = 0; p != NULL;
		     p = ec_parsed_get_parent(p), len++)
			item->path[len] = p->node;
	}

	item->type = type;
	item->node = node;
	if (add != NULL) {
		item->add = ec_strdup(add);
		if (item->add == NULL)
			goto fail;
	}

	return item;

fail:
	if (item != NULL) {
		ec_free(item->path);
		ec_free(item->add);
	}
	ec_completed_item_free(item);

	return NULL;
}

static int ec_completed_add_item(struct ec_completed *completed,
				struct ec_completed_item *item)
{
	size_t n;

	if (item->add != NULL) {
		if (completed->smallest_start == NULL) {
			completed->smallest_start = ec_strdup(item->add);
			if (completed->smallest_start == NULL)
				return -ENOMEM;
		} else {
			n = strcmp_count(item->add,
				completed->smallest_start);
			completed->smallest_start[n] = '\0';
		}
		completed->count_match++;
	}

	TAILQ_INSERT_TAIL(&completed->match_items, item, next);
	completed->count++;

	return 0;
}

int ec_completed_add_match(struct ec_completed *completed,
			struct ec_parsed *state,
			const struct ec_node *node, const char *add)
{
	struct ec_completed_item *item;
	int ret;

	item = ec_completed_item(EC_MATCH, state, node, add);
	if (item == NULL)
		return -ENOMEM;

	ret = ec_completed_add_item(completed, item);
	if (ret < 0) {
		ec_completed_item_free(item);
		return ret;
	}

	return 0;
}

int ec_completed_add_no_match(struct ec_completed *completed,
			struct ec_parsed *state, const struct ec_node *node)
{
	struct ec_completed_item *item;
	int ret;

	item = ec_completed_item(EC_NO_MATCH, state, node, NULL);
	if (item == NULL)
		return -ENOMEM;

	ret = ec_completed_add_item(completed, item);
	if (ret < 0) {
		ec_completed_item_free(item);
		return ret;
	}

	return 0;
}

void ec_completed_item_free(struct ec_completed_item *item)
{
	ec_free(item->add);
	ec_free(item->path);
	ec_free(item);
}

/* default completion function: return a no-match element */
struct ec_completed *ec_node_default_complete(const struct ec_node *gen_node,
					struct ec_parsed *state,
					const struct ec_strvec *strvec)
{
	struct ec_completed *completed;

	(void)strvec;
	(void)state;

	completed = ec_completed();
	if (completed == NULL)
		return NULL;

	if (ec_strvec_len(strvec) != 1)
		return completed;

	if (ec_completed_add_no_match(completed, state, gen_node) < 0) {
		ec_completed_free(completed);
		return NULL;
	}

	return completed;
}

void ec_completed_merge(struct ec_completed *completed1,
	struct ec_completed *completed2)
{
	struct ec_completed_item *item;

	assert(completed1 != NULL);
	assert(completed2 != NULL);

	while (!TAILQ_EMPTY(&completed2->match_items)) {
		item = TAILQ_FIRST(&completed2->match_items);
		TAILQ_REMOVE(&completed2->match_items, item, next);
		ec_completed_add_item(completed1, item);
	}

	ec_completed_free(completed2);
}

void ec_completed_free(struct ec_completed *completed)
{
	struct ec_completed_item *item;

	if (completed == NULL)
		return;

	while (!TAILQ_EMPTY(&completed->match_items)) {
		item = TAILQ_FIRST(&completed->match_items);
		TAILQ_REMOVE(&completed->match_items, item, next);
		ec_completed_item_free(item);
	}
	ec_free(completed->smallest_start);
	ec_free(completed);
}

void ec_completed_dump(FILE *out, const struct ec_completed *completed)
{
	struct ec_completed_item *item;

	if (completed == NULL || completed->count == 0) {
		fprintf(out, "no completion\n");
		return;
	}

	fprintf(out, "completion: count=%u match=%u smallest_start=<%s>\n",
		completed->count, completed->count_match,
		completed->smallest_start);

	TAILQ_FOREACH(item, &completed->match_items, next) {
		fprintf(out, "add=<%s>, node=%p, node_type=%s\n",
			item->add, item->node, item->node->type->name);
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
	enum ec_completed_type type)
{
	unsigned int count = 0;

	if (completed == NULL)
		return count;

	if (type & EC_MATCH)
		count += completed->count_match;
	if (type & EC_NO_MATCH)
		count += (completed->count - completed->count_match); //XXX

	return count;
}

struct ec_completed_iter *
ec_completed_iter(struct ec_completed *completed,
	enum ec_completed_type type)
{
	struct ec_completed_iter *iter;

	iter = ec_calloc(1, sizeof(*iter));
	if (iter == NULL)
		return NULL;

	iter->completed = completed;
	iter->type = type;
	iter->cur_item = NULL;

	return iter;
}

const struct ec_completed_item *ec_completed_iter_next(
	struct ec_completed_iter *iter)
{
	const struct ec_completed *completed = iter->completed;

	if (completed == NULL)
		return NULL;

	do {
		if (iter->cur_item == NULL)
			iter->cur_item = TAILQ_FIRST(&completed->match_items);
		else
			iter->cur_item = TAILQ_NEXT(iter->cur_item, next);

		if (iter->cur_item == NULL)
			break;

		if (iter->cur_item->add == NULL &&
				(iter->type & EC_NO_MATCH))
			break;

		if (iter->cur_item->add != NULL &&
				(iter->type & EC_MATCH))
			break;

	} while (iter->cur_item != NULL);

	return iter->cur_item;
}

void ec_completed_iter_free(struct ec_completed_iter *iter)
{
	ec_free(iter);
}
