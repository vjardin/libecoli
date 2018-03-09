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
#include <ecoli_string.h>
#include <ecoli_strvec.h>
#include <ecoli_keyval.h>
#include <ecoli_log.h>
#include <ecoli_node.h>
#include <ecoli_parsed.h>
#include <ecoli_completed.h>

struct ec_completed_item {
	TAILQ_ENTRY(ec_completed_item) next;
	enum ec_completed_type type;
	const struct ec_node *node;
	struct ec_completed_group *grp;
	char *start;      /* the initial token */
	char *full;       /* the full token after completion */
	char *completion; /* chars that are added, NULL if not applicable */
	char *display;    /* what should be displayed by help/completers */
	struct ec_keyval *attrs;
};

struct ec_completed *ec_completed(struct ec_parsed *state)
{
	struct ec_completed *completed = NULL;

	completed = ec_calloc(1, sizeof(*completed));
	if (completed == NULL)
		goto fail;

	completed->attrs = ec_keyval();
	if (completed->attrs == NULL)
		goto fail;

	TAILQ_INIT(&completed->groups);

	completed->cur_state = state;

	return completed;

 fail:
	if (completed != NULL)
		ec_keyval_free(completed->attrs);
	ec_free(completed);

	return NULL;
}

struct ec_parsed *ec_completed_get_state(struct ec_completed *completed)
{
	return completed->cur_state;
}

int
ec_node_complete_child(const struct ec_node *node,
		struct ec_completed *completed,
		const struct ec_strvec *strvec)
{
	struct ec_parsed *child_state, *cur_state;
	struct ec_completed_group *cur_group;
	int ret;

	if (node->type->complete == NULL)
		return -ENOTSUP;

	/* save previous parse state, prepare child state */
	cur_state = completed->cur_state;
	child_state = ec_parsed(node);
	if (child_state == NULL)
		return -ENOMEM;

	if (cur_state != NULL)
		ec_parsed_add_child(cur_state, child_state);
	completed->cur_state = child_state;
	cur_group = completed->cur_group;
	completed->cur_group = NULL;

	/* fill the completed struct with items */
	ret = node->type->complete(node, completed, strvec);

	/* restore parent parse state */
	if (cur_state != NULL) {
		ec_parsed_del_child(cur_state, child_state);
		assert(!ec_parsed_has_child(child_state));
	}
	ec_parsed_free(child_state);
	completed->cur_state = cur_state;
	completed->cur_group = cur_group;

	if (ret < 0)
		return ret;

	return 0;
}

struct ec_completed *ec_node_complete_strvec(const struct ec_node *node,
	const struct ec_strvec *strvec)
{
	struct ec_completed *completed = NULL;
	int ret;

	completed = ec_completed(NULL);
	if (completed == NULL)
		goto fail;

	ret = ec_node_complete_child(node, completed, strvec);
	if (ret < 0)
		goto fail;

	return completed;

fail:
	ec_completed_free(completed);
	return NULL;
}

struct ec_completed *ec_node_complete(const struct ec_node *node,
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

static struct ec_completed_group *
ec_completed_group(const struct ec_node *node, struct ec_parsed *parsed)
{
	struct ec_completed_group *grp = NULL;

	grp = ec_calloc(1, sizeof(*grp));
	if (grp == NULL)
		return NULL;

	grp->attrs = ec_keyval();
	if (grp->attrs == NULL)
		goto fail;

	grp->state = ec_parsed_dup(parsed);
	if (grp->state == NULL)
		goto fail;

	grp->node = node;
	TAILQ_INIT(&grp->items);

	return grp;

fail:
	if (grp != NULL) {
		ec_parsed_free(grp->state);
		ec_keyval_free(grp->attrs);
	}
	ec_free(grp);
	return NULL;
}

static struct ec_completed_item *
ec_completed_item(const struct ec_node *node, enum ec_completed_type type,
		const char *start, const char *full)
{
	struct ec_completed_item *item = NULL;
	struct ec_keyval *attrs = NULL;
	char *comp_cp = NULL, *start_cp = NULL;
	char *full_cp = NULL, *display_cp = NULL;

	if (type == EC_COMP_UNKNOWN && full != NULL) {
		errno = EINVAL;
		return NULL;
	}
	if (type != EC_COMP_UNKNOWN && full == NULL) {
		errno = EINVAL;
		return NULL;
	}

	item = ec_calloc(1, sizeof(*item));
	if (item == NULL)
		goto fail;

	attrs = ec_keyval();
	if (attrs == NULL)
		goto fail;

	if (start != NULL) {
		start_cp = ec_strdup(start);
		if (start_cp == NULL)
			goto fail;

		if (ec_str_startswith(full, start)) {
			comp_cp = ec_strdup(&full[strlen(start)]);
			if (comp_cp == NULL)
				goto fail;
		}
	}
	if (full != NULL) {
		full_cp = ec_strdup(full);
		if (full_cp == NULL)
			goto fail;
		display_cp = ec_strdup(full);
		if (display_cp == NULL)
			goto fail;
	}

	item->node = node;
	item->type = type;
	item->start = start_cp;
	item->full = full_cp;
	item->completion = comp_cp;
	item->display = display_cp;
	item->attrs = attrs;

	return item;

fail:
	ec_keyval_free(attrs);
	ec_free(comp_cp);
	ec_free(start_cp);
	ec_free(full_cp);
	ec_free(display_cp);
	ec_free(item);

	return NULL;
}

int ec_completed_item_set_display(struct ec_completed_item *item,
				const char *display)
{
	char *display_copy = NULL;
	int ret = 0;

	if (item == NULL || display == NULL ||
			item->type == EC_COMP_UNKNOWN)
		return -EINVAL;

	display_copy = ec_strdup(display);
	if (display_copy == NULL)
		goto fail;

	ec_free(item->display);
	item->display = display_copy;

	return 0;

fail:
	ec_free(display_copy);
	return ret;
}

int
ec_completed_item_set_completion(struct ec_completed_item *item,
				const char *completion)
{
	char *completion_copy = NULL;
	int ret = 0;

	if (item == NULL || completion == NULL ||
			item->type == EC_COMP_UNKNOWN)
		return -EINVAL;

	ret = -ENOMEM;
	completion_copy = ec_strdup(completion);
	if (completion_copy == NULL)
		goto fail;

	ec_free(item->completion);
	item->completion = completion_copy;

	return 0;

fail:
	ec_free(completion_copy);
	return ret;
}

int
ec_completed_item_set_str(struct ec_completed_item *item,
			const char *str)
{
	char *str_copy = NULL;
	int ret = 0;

	if (item == NULL || str == NULL ||
			item->type == EC_COMP_UNKNOWN)
		return -EINVAL;

	ret = -ENOMEM;
	str_copy = ec_strdup(str);
	if (str_copy == NULL)
		goto fail;

	ec_free(item->full);
	item->full = str_copy;

	return 0;

fail:
	ec_free(str_copy);
	return ret;
}

static int
ec_completed_item_add(struct ec_completed *completed,
		struct ec_completed_item *item)
{
	if (completed == NULL || item == NULL || item->node == NULL)
		return -EINVAL;

	switch (item->type) {
	case EC_COMP_UNKNOWN:
		completed->count_unknown++;
		break;
	case EC_COMP_FULL:
		completed->count_full++;
		break;
	case EC_COMP_PARTIAL:
		completed->count_partial++;
		break;
	default:
		return -EINVAL;
	}

	if (completed->cur_group == NULL) {
		struct ec_completed_group *grp;

		grp = ec_completed_group(item->node, completed->cur_state);
		if (grp == NULL)
			return -ENOMEM;
		TAILQ_INSERT_TAIL(&completed->groups, grp, next);
		completed->cur_group = grp;
	}

	completed->count++;
	TAILQ_INSERT_TAIL(&completed->cur_group->items, item, next);
	item->grp = completed->cur_group;

	return 0;
}

const char *
ec_completed_item_get_str(const struct ec_completed_item *item)
{
	return item->full;
}

const char *
ec_completed_item_get_display(const struct ec_completed_item *item)
{
	return item->display;
}

const char *
ec_completed_item_get_completion(const struct ec_completed_item *item)
{
	return item->completion;
}

enum ec_completed_type
ec_completed_item_get_type(const struct ec_completed_item *item)
{
	return item->type;
}

const struct ec_node *
ec_completed_item_get_node(const struct ec_completed_item *item)
{
	return item->node;
}

const struct ec_completed_group *
ec_completed_item_get_grp(const struct ec_completed_item *item)
{
	return item->grp;
}

static void
ec_completed_item_free(struct ec_completed_item *item)
{
	if (item == NULL)
		return;

	ec_free(item->full);
	ec_free(item->start);
	ec_free(item->completion);
	ec_free(item->display);
	ec_keyval_free(item->attrs);
	ec_free(item);
}

int ec_completed_add_item(struct ec_completed *completed,
			const struct ec_node *node,
			struct ec_completed_item **p_item,
			enum ec_completed_type type,
			const char *start, const char *full)
{
	struct ec_completed_item *item = NULL;
	int ret;

	item = ec_completed_item(node, type, start, full);
	if (item == NULL)
		return -1;

	ret = ec_completed_item_add(completed, item);
	if (ret < 0)
		goto fail;

	if (p_item != NULL)
		*p_item = item;

	return 0;

fail:
	ec_completed_item_free(item);

	return -1;
}

/* default completion function: return a no-match element */
int
ec_node_default_complete(const struct ec_node *gen_node, // XXX rename in nomatch
			struct ec_completed *completed,
			const struct ec_strvec *strvec)
{
	int ret;

	if (ec_strvec_len(strvec) != 1)
		return 0;

	ret = ec_completed_add_item(completed, gen_node, NULL,
				EC_COMP_UNKNOWN, NULL, NULL);
	if (ret < 0)
		return ret;

	return 0;
}

static void ec_completed_group_free(struct ec_completed_group *grp)
{
	struct ec_completed_item *item;

	if (grp == NULL)
		return;

	while (!TAILQ_EMPTY(&grp->items)) {
		item = TAILQ_FIRST(&grp->items);
		TAILQ_REMOVE(&grp->items, item, next);
		ec_completed_item_free(item);
	}
	ec_parsed_free(ec_parsed_get_root(grp->state));
	ec_keyval_free(grp->attrs);
	ec_free(grp);
}

void ec_completed_free(struct ec_completed *completed)
{
	struct ec_completed_group *grp;

	if (completed == NULL)
		return;

	while (!TAILQ_EMPTY(&completed->groups)) {
		grp = TAILQ_FIRST(&completed->groups);
		TAILQ_REMOVE(&completed->groups, grp, next);
		ec_completed_group_free(grp);
	}
	ec_keyval_free(completed->attrs);
	ec_free(completed);
}

void ec_completed_dump(FILE *out, const struct ec_completed *completed)
{
	struct ec_completed_group *grp;
	struct ec_completed_item *item;

	if (completed == NULL || completed->count == 0) {
		fprintf(out, "no completion\n");
		return;
	}

	fprintf(out, "completion: count=%u full=%u partial=%u unknown=%u\n",
		completed->count, completed->count_full,
		completed->count_partial,  completed->count_unknown);

	TAILQ_FOREACH(grp, &completed->groups, next) {
		fprintf(out, "node=%p, node_type=%s\n",
			grp->node, grp->node->type->name);
		TAILQ_FOREACH(item, &grp->items, next) {
			const char *typestr;

			switch (item->type) {
			case EC_COMP_UNKNOWN: typestr = "unknown"; break;
			case EC_COMP_FULL: typestr = "full"; break;
			case EC_COMP_PARTIAL: typestr = "partial"; break;
			default: typestr = "unknown"; break;
			}

			fprintf(out, "  type=%s str=<%s> comp=<%s> disp=<%s>\n",
				typestr, item->full, item->completion,
				item->display);
		}
	}
}

int ec_completed_merge(struct ec_completed *to,
		struct ec_completed *from)
{
	struct ec_completed_group *grp;

	while (!TAILQ_EMPTY(&from->groups)) {
		grp = TAILQ_FIRST(&from->groups);
		TAILQ_REMOVE(&from->groups, grp, next);
		TAILQ_INSERT_TAIL(&to->groups, grp, next);
	}
	to->count += from->count;
	to->count_full += from->count_full;
	to->count_partial += from->count_partial;
	to->count_unknown += from->count_unknown;

	ec_completed_free(from);
	return 0;
}

unsigned int ec_completed_count(
	const struct ec_completed *completed,
	enum ec_completed_type type)
{
	unsigned int count = 0;

	if (completed == NULL)
		return count;

	if (type & EC_COMP_FULL)
		count += completed->count_full;
	if (type & EC_COMP_PARTIAL)
		count += completed->count_partial;
	if (type & EC_COMP_UNKNOWN)
		count += completed->count_unknown;

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

struct ec_completed_item *ec_completed_iter_next(
	struct ec_completed_iter *iter)
{
	struct ec_completed *completed;
	struct ec_completed_group *cur_node;
	struct ec_completed_item *cur_match;

	if (iter == NULL)
		return NULL;
	completed = iter->completed;
	if (completed == NULL)
		return NULL;

	cur_node = iter->cur_node;
	cur_match = iter->cur_match;

	/* first call */
	if (cur_node == NULL) {
		TAILQ_FOREACH(cur_node, &completed->groups, next) {
			TAILQ_FOREACH(cur_match, &cur_node->items, next) {
				if (cur_match != NULL &&
						cur_match->type & iter->type)
					goto found;
			}
		}
		return NULL;
	} else {
		cur_match = TAILQ_NEXT(cur_match, next);
		if (cur_match != NULL &&
				cur_match->type & iter->type)
			goto found;
		cur_node = TAILQ_NEXT(cur_node, next);
		while (cur_node != NULL) {
			cur_match = TAILQ_FIRST(&cur_node->items);
			if (cur_match != NULL &&
					cur_match->type & iter->type)
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
