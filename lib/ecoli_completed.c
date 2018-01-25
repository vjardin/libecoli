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

struct ec_completed_item {
	TAILQ_ENTRY(ec_completed_item) next;
	enum ec_completed_type type;
	const struct ec_node *node;
	struct ec_completed_group *grp;
	char *str;
	char *display;
	struct ec_keyval *attrs;
};

struct ec_completed *ec_completed(void)
{
	struct ec_completed *completed = NULL;

	completed = ec_calloc(1, sizeof(*completed));
	if (completed == NULL)
		goto fail;

	TAILQ_INIT(&completed->groups);

	completed->attrs = ec_keyval();
	if (completed->attrs == NULL)
		goto fail;

	completed->cur_state = NULL;

	return completed;

 fail:
	if (completed != NULL)
		ec_keyval_free(completed->attrs);
	ec_free(completed);

	return NULL;
}

struct ec_parsed *ec_completed_cur_parse_state(struct ec_completed *completed)
{
	return completed->cur_state;
}

int
ec_node_complete_child(struct ec_node *node, struct ec_completed *completed,
			const struct ec_strvec *strvec)
{
	struct ec_parsed *child_state, *cur_state;
	struct ec_completed_group *cur_group;
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

	/* save previous parse state, prepare child state */
	cur_state = completed->cur_state;
	child_state = ec_parsed();
	if (child_state == NULL)
		return -ENOMEM;

	if (cur_state != NULL)
		ec_parsed_add_child(cur_state, child_state);
	ec_parsed_set_node(child_state, node);
	completed->cur_state = child_state;
	cur_group = completed->cur_group;
	completed->cur_group = NULL;

	/* complete */
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

#if 0 // XXX dump
	printf("----------------------------------------------------------\n");
	ec_node_dump(stdout, node);
	ec_strvec_dump(stdout, strvec);
	ec_completed_dump(stdout, completed);
#endif

	return 0;
}

struct ec_completed *ec_node_complete_strvec(struct ec_node *node,
	const struct ec_strvec *strvec)
{
	struct ec_completed *completed = NULL;
	int ret;

	completed = ec_completed();
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

static struct ec_completed_group *
ec_completed_group(const struct ec_node *node, struct ec_parsed *parsed)
{
	struct ec_completed_group *grp = NULL;

	grp = ec_calloc(1, sizeof(*grp));
	if (grp == NULL)
		return NULL;

	grp->state = ec_parsed_dup(parsed);
	if (grp->state == NULL)
		goto fail;

	grp->node = node;
	TAILQ_INIT(&grp->items);

	return grp;

fail:
	if (grp != NULL)
		ec_parsed_free(grp->state);
	ec_free(grp);
	return NULL;
}

struct ec_completed_item *
ec_completed_item(const struct ec_node *node)
{
	struct ec_completed_item *item = NULL;

	item = ec_calloc(1, sizeof(*item));
	if (item == NULL)
		goto fail;

	item->attrs = ec_keyval();
	if (item->attrs == NULL)
		goto fail;

	item->type = EC_COMP_UNKNOWN;
	item->node = node;

	return item;

fail:
	if (item != NULL) {
		ec_free(item->str);
		ec_free(item->display);
		ec_keyval_free(item->attrs);
	}
	ec_free(item);

	return NULL;
}

int
ec_completed_item_set(struct ec_completed_item *item,
		enum ec_completed_type type, const char *str)
{
	char *str_copy = NULL;
	char *display_copy = NULL;
	int ret = 0;

	if (item == NULL)
		return -EINVAL;
	if (item->str != NULL)
		return -EEXIST;

	switch (type) {
	case EC_COMP_UNKNOWN:
		if (str != NULL)
			return -EINVAL;
		break;
	case EC_COMP_FULL:
	case EC_PARTIAL_MATCH:
		if (str == NULL)
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	if (str != NULL) {
		ret = -ENOMEM;
		str_copy = ec_strdup(str);
		if (str_copy == NULL)
			goto fail;
		display_copy = ec_strdup(str);
		if (display_copy == NULL)
			goto fail;
	}

	item->type = type;
	item->str = str_copy;
	item->display = display_copy;
	return 0;

fail:
	ec_free(str_copy);
	ec_free(display_copy);
	return ret;
}

int ec_completed_item_set_display(struct ec_completed_item *item,
				const char *display)
{
	char *display_copy = NULL;
	int ret = 0;

	if (item == NULL || display == NULL ||
			item->type == EC_COMP_UNKNOWN || item->str == NULL)
		return -EINVAL;

	ret = -ENOMEM;
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

// XXX refactor ec_completed_item(), ec_completed_item_add(), ec_completed_item_set* 
int
ec_completed_item_add(struct ec_completed *completed,
		struct ec_completed_item *item)
{
	if (completed == NULL || item == NULL || item->node == NULL)
		return -EINVAL;

	switch (item->type) {
	case EC_COMP_UNKNOWN:
		break;
	case EC_COMP_FULL:
	case EC_PARTIAL_MATCH:
		completed->count_match++; //XXX
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
	return item->str;
}

const char *
ec_completed_item_get_display(const struct ec_completed_item *item)
{
	return item->display;
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

void ec_completed_item_free(struct ec_completed_item *item)
{
	if (item == NULL)
		return;

	ec_free(item->str);
	ec_free(item->display);
	ec_keyval_free(item->attrs);
	ec_free(item);
}

/* default completion function: return a no-match element */
int
ec_node_default_complete(const struct ec_node *gen_node, // XXX rename in nomatch
			struct ec_completed *completed,
			const struct ec_strvec *strvec)
{
	struct ec_completed_item *item = NULL;
	int ret;

	if (ec_strvec_len(strvec) != 1)
		return 0;

	item = ec_completed_item(gen_node);
	if (item == NULL)
		return -ENOMEM;
	ret = ec_completed_item_set(item, EC_COMP_UNKNOWN, NULL);
	if (ret < 0) {
		ec_completed_item_free(item);
		return ret;
	}
	ret = ec_completed_item_add(completed, item);
	if (ret < 0) {
		ec_completed_item_free(item);
		return ret;
	}

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

	fprintf(out, "completion: count=%u match=%u\n",
		completed->count, completed->count_match);

	TAILQ_FOREACH(grp, &completed->groups, next) {
		fprintf(out, "node=%p, node_type=%s\n",
			grp->node, grp->node->type->name);
		TAILQ_FOREACH(item, &grp->items, next) {
			const char *typestr;

			switch (item->type) {
			case EC_COMP_UNKNOWN: typestr = "no-match"; break;
			case EC_COMP_FULL: typestr = "match"; break;
			case EC_PARTIAL_MATCH: typestr = "partial-match"; break;
			default: typestr = "unknown"; break;
			}

			fprintf(out, "  type=%s str=<%s> disp=<%s>\n",
				typestr, item->str, item->display);
		}
	}
}

unsigned int ec_completed_count(
	const struct ec_completed *completed,
	enum ec_completed_type type)
{
	unsigned int count = 0;

	if (completed == NULL)
		return count;

	if (type & EC_COMP_FULL)
		count += completed->count_match;
	if (type & EC_COMP_UNKNOWN)
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

const struct ec_completed_item *ec_completed_iter_next(
	struct ec_completed_iter *iter)
{
	const struct ec_completed *completed = iter->completed;
	const struct ec_completed_group *cur_node;
	const struct ec_completed_item *cur_match;

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
