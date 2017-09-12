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

	TAILQ_INIT(&completed->nodes);

	return completed;
}

/* XXX on error, states are not freed ?
 * they can be left in a bad state and should not be reused */
int
ec_node_complete_child(struct ec_node *node,
		struct ec_completed *completed,
		struct ec_parsed *parsed_state,
		const struct ec_strvec *strvec)
{
	struct ec_parsed *child_state = NULL;
	int ret;

	/* build the node if required */
	if (node->type->build != NULL) {
		if ((node->flags & EC_NODE_F_BUILT) == 0) {
			ret = node->type->build(node);
			if (ret < 0)
				return ret;
		}
	}
	node->flags |= EC_NODE_F_BUILT;

	if (node->type->complete == NULL)
		return -ENOTSUP;

	child_state = ec_parsed();
	if (child_state == NULL)
		return -ENOMEM;
	child_state->node = node;
	ec_parsed_add_child(parsed_state, child_state);

	ret = ec_completed_add_node(completed, node);
	if (ret < 0)
		return ret;

	ret = node->type->complete(node, completed,
				child_state, strvec);
	if (ret < 0)
		return ret;

#if 0 // XXX dump
	printf("----------------------------------------------------------\n");
	ec_node_dump(stdout, node);
	ec_strvec_dump(stdout, strvec);
	ec_completed_dump(stdout, completed);
	ec_parsed_dump(stdout, parsed_state);
#endif

	ec_parsed_del_child(parsed_state, child_state);
	assert(TAILQ_EMPTY(&child_state->children));
	ec_parsed_free(child_state);

	return 0;
}

struct ec_completed *ec_node_complete_strvec(struct ec_node *node,
	const struct ec_strvec *strvec)
{
	struct ec_parsed *parsed_state = NULL;
	struct ec_completed *completed = NULL;
	int ret;

	parsed_state = ec_parsed();
	if (parsed_state == NULL)
		goto fail;

	completed = ec_completed();
	if (completed == NULL)
		goto fail;

	ret = ec_node_complete_child(node, completed,
				parsed_state, strvec);
	if (ret < 0)
		goto fail;

	ec_parsed_free(parsed_state);

	return completed;

fail:
	ec_parsed_free(parsed_state);
	ec_completed_free(completed);
	return NULL;
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

static struct ec_completed_node *
ec_completed_node(const struct ec_node *node)
{
	struct ec_completed_node *compnode = NULL;

	compnode = ec_calloc(1, sizeof(*compnode));
	if (compnode == NULL)
		return NULL;

	compnode->node = node;
	TAILQ_INIT(&compnode->matches);

	return compnode;
}

static struct ec_completed_match *
ec_completed_match(enum ec_completed_type type, struct ec_parsed *state,
		const struct ec_node *node, const char *add)
{
	struct ec_completed_match *item = NULL;

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
	ec_completed_match_free(item);

	return NULL;
}

int
ec_completed_add_match(struct ec_completed *completed,
		struct ec_parsed *parsed_state,
		const struct ec_node *node, const char *add)
{
	struct ec_completed_node *compnode = NULL;
	struct ec_completed_match *item = NULL;
	int ret = -ENOMEM;
	size_t n;

	TAILQ_FOREACH(compnode, &completed->nodes, next) {
		if (compnode->node == node)
			break;
	}
	if (compnode == NULL)
		return -ENOENT;

	item = ec_completed_match(EC_MATCH, parsed_state, node, add);
	if (item == NULL)
		goto fail;

	if (item->add != NULL) {
		if (completed->smallest_start == NULL) {
			completed->smallest_start = ec_strdup(item->add);
			if (completed->smallest_start == NULL)
				goto fail;
		} else {
			n = strcmp_count(item->add,
				completed->smallest_start);
			completed->smallest_start[n] = '\0';
		}
		completed->count_match++;
	}

	TAILQ_INSERT_TAIL(&compnode->matches, item, next);
	completed->count++;

	return 0;

fail:
	ec_completed_match_free(item);
	return ret;
}

int
ec_completed_add_node(struct ec_completed *completed,
		const struct ec_node *node)
{
	struct ec_completed_node *item = NULL;

	item = ec_completed_node(node);
	if (item == NULL)
		return -ENOMEM;

	TAILQ_INSERT_TAIL(&completed->nodes, item, next);
	return 0;
}

void ec_completed_match_free(struct ec_completed_match *item)
{
	ec_free(item->add);
	ec_free(item->path);
	ec_free(item);
}

/* default completion function: return a no-item element */
int
ec_node_default_complete(const struct ec_node *gen_node,
			struct ec_completed *completed,
			struct ec_parsed *parsed,
			const struct ec_strvec *strvec)
{
	(void)gen_node;
	(void)completed;
	(void)parsed;

	if (ec_strvec_len(strvec) != 1) //XXX needed?
		return 0;

	return 0;
}

void ec_completed_free(struct ec_completed *completed)
{
	struct ec_completed_node *compnode;
	struct ec_completed_match *item;

	if (completed == NULL)
		return;

	while (!TAILQ_EMPTY(&completed->nodes)) {
		compnode = TAILQ_FIRST(&completed->nodes);
		TAILQ_REMOVE(&completed->nodes, compnode, next);

		while (!TAILQ_EMPTY(&compnode->matches)) {
			item = TAILQ_FIRST(&compnode->matches);
			TAILQ_REMOVE(&compnode->matches, item, next);
			ec_completed_match_free(item);
		}
		ec_free(compnode);
	}
	ec_free(completed->smallest_start);
	ec_free(completed);
}

void ec_completed_dump(FILE *out, const struct ec_completed *completed)
{
	struct ec_completed_node *compnode;
	struct ec_completed_match *item;

	if (completed == NULL || completed->count == 0) {
		fprintf(out, "no completion\n");
		return;
	}

	fprintf(out, "completion: count=%u match=%u smallest_start=<%s>\n",
		completed->count, completed->count_match,
		completed->smallest_start);

	TAILQ_FOREACH(compnode, &completed->nodes, next) {
		fprintf(out, "node=%p, node_type=%s\n",
			compnode->node, compnode->node->type->name);
		TAILQ_FOREACH(item, &compnode->matches, next) {
			fprintf(out, "add=<%s>\n", item->add); /* XXX comp type */
		}
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
	iter->cur_node = NULL;
	iter->cur_match = NULL;

	return iter;
}

const struct ec_completed_match *ec_completed_iter_next(
	struct ec_completed_iter *iter)
{
	const struct ec_completed *completed = iter->completed;
	const struct ec_completed_node *cur_node;
	const struct ec_completed_match *cur_match;

	if (completed == NULL)
		return NULL;

	cur_node = iter->cur_node;
	cur_match = iter->cur_match;

	/* first call */
	if (cur_node == NULL) {
		TAILQ_FOREACH(cur_node, &completed->nodes, next) {
			TAILQ_FOREACH(cur_match, &cur_node->matches, next) {
				if (cur_match != NULL)
					goto found;
			}
		}
		return NULL;
	} else {
		cur_match = TAILQ_NEXT(cur_match, next);
		if (cur_match != NULL)
			goto found;
		cur_node = TAILQ_NEXT(cur_node, next);
		while (cur_node != NULL) {
			cur_match = TAILQ_FIRST(&cur_node->matches);
			if (cur_match != NULL)
				goto found;
			cur_node = TAILQ_NEXT(cur_node, next);
		}
		return NULL;
	}

found:
	iter->cur_node = cur_node;
	iter->cur_match = cur_match;

	return iter->cur_match;
}

void ec_completed_iter_free(struct ec_completed_iter *iter)
{
	ec_free(iter);
}
