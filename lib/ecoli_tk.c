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
#include <ecoli_tk.h>

struct ec_tk *ec_tk_new(const char *id, const struct ec_tk_ops *ops,
	size_t size)
{
	struct ec_tk *tk = NULL;
	char buf[256]; // XXX

	assert(size >= sizeof(*tk));

	ec_log(EC_LOG_DEBUG, "create node type=%s id=%s\n", ops->typename, id);

	tk = ec_calloc(1, size);
	if (tk == NULL)
		goto fail;

	TAILQ_INIT(&tk->children);
	tk->ops = ops;
	tk->refcnt = 1;

	if (id != NULL) {
		tk->id = ec_strdup(id);
		if (tk->id == NULL)
			goto fail;
	}

	snprintf(buf, sizeof(buf), "<%s>", ops->typename);
	tk->desc = ec_strdup(buf); // XXX ec_asprintf ?
	if (tk->desc == NULL)
		goto fail;

	tk->attrs = ec_keyval_new();
	if (tk->attrs == NULL)
		goto fail;

	return tk;

 fail:
	ec_tk_free(tk);
	return NULL;
}

void ec_tk_free(struct ec_tk *tk)
{
	if (tk == NULL)
		return;

	assert(tk->refcnt > 0);

	if (--tk->refcnt > 0)
		return;

	if (tk->ops != NULL && tk->ops->free_priv != NULL)
		tk->ops->free_priv(tk);
	ec_free(tk->id);
	ec_free(tk->desc);
	ec_free(tk->attrs);
	ec_free(tk);
}

struct ec_tk *ec_tk_clone(struct ec_tk *tk)
{
	if (tk != NULL)
		tk->refcnt++;
	return tk;
}

struct ec_tk *ec_tk_find(struct ec_tk *tk, const char *id)
{
	struct ec_tk *child, *ret;
	const char *tk_id = ec_tk_id(tk);

	if (id != NULL && tk_id != NULL && !strcmp(tk_id, id))
		return tk;

	TAILQ_FOREACH(child, &tk->children, next) {
		ret = ec_tk_find(child, id);
		if (ret != NULL)
			return ret;
	}

	return NULL;
}

struct ec_keyval *ec_tk_attrs(const struct ec_tk *tk)
{
	return tk->attrs;
}

const char *ec_tk_id(const struct ec_tk *tk)
{
	return tk->id;
}

struct ec_tk *ec_tk_parent(const struct ec_tk *tk)
{
	return tk->parent;
}

struct ec_parsed_tk *ec_tk_parse(struct ec_tk *tk, const char *str)
{
	struct ec_strvec *strvec = NULL;
	struct ec_parsed_tk *parsed_tk;

	errno = ENOMEM;
	strvec = ec_strvec_new();
	if (strvec == NULL)
		goto fail;

	if (ec_strvec_add(strvec, str) < 0)
		goto fail;

	parsed_tk = ec_tk_parse_tokens(tk, strvec);
	if (parsed_tk == NULL)
		goto fail;

	ec_strvec_free(strvec);
	return parsed_tk;

 fail:
	ec_strvec_free(strvec);
	return NULL;
}

struct ec_parsed_tk *ec_tk_parse_tokens(struct ec_tk *tk,
	const struct ec_strvec *strvec)
{
	struct ec_parsed_tk *parsed_tk;
	int ret;

	/* build the node if required */
	if (tk->ops->build != NULL) {
		if ((tk->flags & EC_TK_F_BUILT) == 0) {
			ret = tk->ops->build(tk);
			if (ret < 0) {
				errno = -ret;
				return NULL;
			}
		}
	}
	tk->flags |= EC_TK_F_BUILT;

	if (tk->ops->parse == NULL) {
		errno = ENOTSUP;
		return NULL;
	}

	parsed_tk = tk->ops->parse(tk, strvec);

	return parsed_tk;
}


struct ec_parsed_tk *ec_parsed_tk_new(void)
{
	struct ec_parsed_tk *parsed_tk = NULL;

	parsed_tk = ec_calloc(1, sizeof(*parsed_tk));
	if (parsed_tk == NULL)
		goto fail;

	TAILQ_INIT(&parsed_tk->children);

	return parsed_tk;

 fail:
	return NULL;
}

void ec_parsed_tk_set_match(struct ec_parsed_tk *parsed_tk,
	const struct ec_tk *tk, struct ec_strvec *strvec)
{
	parsed_tk->tk = tk;
	parsed_tk->strvec = strvec;
}

void ec_parsed_tk_free_children(struct ec_parsed_tk *parsed_tk)
{
	struct ec_parsed_tk *child;

	if (parsed_tk == NULL)
		return;

	while (!TAILQ_EMPTY(&parsed_tk->children)) {
		child = TAILQ_FIRST(&parsed_tk->children);
		TAILQ_REMOVE(&parsed_tk->children, child, next);
		ec_parsed_tk_free(child);
	}
}

void ec_parsed_tk_free(struct ec_parsed_tk *parsed_tk)
{
	if (parsed_tk == NULL)
		return;

	ec_parsed_tk_free_children(parsed_tk);
	ec_strvec_free(parsed_tk->strvec);
	ec_free(parsed_tk);
}

static void __ec_parsed_tk_dump(FILE *out,
	const struct ec_parsed_tk *parsed_tk, size_t indent)
{
	struct ec_parsed_tk *child;
	const struct ec_strvec *vec;
	size_t i;
	const char *id = "None", *typename = "None";

	/* XXX enhance */
	for (i = 0; i < indent; i++)
		fprintf(out, " ");

	if (parsed_tk->tk != NULL) {
		if (parsed_tk->tk->id != NULL)
			id = parsed_tk->tk->id;
		typename = parsed_tk->tk->ops->typename;
	}

	fprintf(out, "tk_type=%s id=%s vec=[", typename, id);
	vec = ec_parsed_tk_strvec(parsed_tk);
	for (i = 0; i < ec_strvec_len(vec); i++)
		fprintf(out, "%s<%s>",
			i == 0 ? "" : ",",
			ec_strvec_val(vec, i));
	fprintf(out, "]\n");

	TAILQ_FOREACH(child, &parsed_tk->children, next)
		__ec_parsed_tk_dump(out, child, indent + 2);
}

void ec_parsed_tk_dump(FILE *out, const struct ec_parsed_tk *parsed_tk)
{
	if (parsed_tk == NULL) {
		fprintf(out, "parsed_tk is NULL, error in parse\n");
		return;
	}
	if (!ec_parsed_tk_matches(parsed_tk)) {
		fprintf(out, "no match\n");
		return;
	}

	__ec_parsed_tk_dump(out, parsed_tk, 0);
}

void ec_parsed_tk_add_child(struct ec_parsed_tk *parsed_tk,
	struct ec_parsed_tk *child)
{
	TAILQ_INSERT_TAIL(&parsed_tk->children, child, next);
}

struct ec_parsed_tk *ec_parsed_tk_find_first(struct ec_parsed_tk *parsed_tk,
	const char *id)
{
	struct ec_parsed_tk *child, *ret;

	if (parsed_tk == NULL)
		return NULL;

	if (parsed_tk->tk != NULL &&
			parsed_tk->tk->id != NULL &&
			!strcmp(parsed_tk->tk->id, id))
		return parsed_tk;

	TAILQ_FOREACH(child, &parsed_tk->children, next) {
		ret = ec_parsed_tk_find_first(child, id);
		if (ret != NULL)
			return ret;
	}

	return NULL;
}

const struct ec_strvec *ec_parsed_tk_strvec(
	const struct ec_parsed_tk *parsed_tk)
{
	if (parsed_tk == NULL || parsed_tk->strvec == NULL)
		return NULL;

	return parsed_tk->strvec;
}

/* number of parsed tokens */
size_t ec_parsed_tk_len(const struct ec_parsed_tk *parsed_tk)
{
	if (parsed_tk == NULL || parsed_tk->strvec == NULL)
		return 0;

	return ec_strvec_len(parsed_tk->strvec);
}

size_t ec_parsed_tk_matches(const struct ec_parsed_tk *parsed_tk)
{
	if (parsed_tk == NULL)
		return 0;

	if (parsed_tk->tk == NULL && TAILQ_EMPTY(&parsed_tk->children))
		return 0;

	return 1;
}

struct ec_completed_tk *ec_completed_tk_new(void)
{
	struct ec_completed_tk *completed_tk = NULL;

	completed_tk = ec_calloc(1, sizeof(*completed_tk));
	if (completed_tk == NULL)
		return NULL;

	TAILQ_INIT(&completed_tk->elts);
	completed_tk->count_match = 0;

	return completed_tk;
}

struct ec_completed_tk_elt *ec_completed_tk_elt_new(const struct ec_tk *tk,
	const char *add)
{
	struct ec_completed_tk_elt *elt = NULL;

	elt = ec_calloc(1, sizeof(*elt));
	if (elt == NULL)
		return NULL;

	elt->tk = tk;
	if (add != NULL) {
		elt->add = ec_strdup(add);
		if (elt->add == NULL) {
			ec_completed_tk_elt_free(elt);
			return NULL;
		}
	}

	return elt;
}

/* XXX define when to use ec_tk_complete() or tk->complete()
 * (same for parse)
 * suggestion: tk->op() is internal, user calls the function
 * other idea: have 2 functions
 */
struct ec_completed_tk *ec_tk_complete(const struct ec_tk *tk,
	const char *str)
{
	struct ec_strvec *strvec = NULL;
	struct ec_completed_tk *completed_tk;

	errno = ENOMEM;
	strvec = ec_strvec_new();
	if (strvec == NULL)
		goto fail;

	if (ec_strvec_add(strvec, str) < 0)
		goto fail;

	completed_tk = ec_tk_complete_tokens(tk, strvec);
	if (completed_tk == NULL)
		goto fail;

	ec_strvec_free(strvec);
	return completed_tk;

 fail:
	ec_strvec_free(strvec);
	return NULL;
}

/* default completion function: return a no-match element */
struct ec_completed_tk *ec_tk_default_complete(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_completed_tk *completed_tk;
	struct ec_completed_tk_elt *completed_tk_elt;

	(void)strvec;

	completed_tk = ec_completed_tk_new();
	if (completed_tk == NULL)
		return NULL;

	completed_tk_elt = ec_completed_tk_elt_new(gen_tk, NULL);
	if (completed_tk_elt == NULL) {
		ec_completed_tk_free(completed_tk);
		return NULL;
	}

	ec_completed_tk_add_elt(completed_tk, completed_tk_elt);

	return completed_tk;
}

struct ec_completed_tk *ec_tk_complete_tokens(const struct ec_tk *tk,
	const struct ec_strvec *strvec)
{
	return tk->ops->complete(tk, strvec);
}

/* count the number of identical chars at the beginning of 2 strings */
static size_t strcmp_count(const char *s1, const char *s2)
{
	size_t i = 0;

	while (s1[i] && s2[i] && s1[i] == s2[i])
		i++;

	return i;
}

void ec_completed_tk_add_elt(
	struct ec_completed_tk *completed_tk, struct ec_completed_tk_elt *elt)
{
	size_t n;

	TAILQ_INSERT_TAIL(&completed_tk->elts, elt, next);
	completed_tk->count++;
	if (elt->add != NULL)
		completed_tk->count_match++;
	if (elt->add != NULL) {
		if (completed_tk->smallest_start == NULL) {
			completed_tk->smallest_start = ec_strdup(elt->add);
		} else {
			n = strcmp_count(elt->add,
				completed_tk->smallest_start);
			completed_tk->smallest_start[n] = '\0';
		}
	}
}

void ec_completed_tk_elt_free(struct ec_completed_tk_elt *elt)
{
	ec_free(elt->add);
	ec_free(elt);
}

void ec_completed_tk_merge(struct ec_completed_tk *completed_tk1,
	struct ec_completed_tk *completed_tk2)
{
	struct ec_completed_tk_elt *elt;

	assert(completed_tk1 != NULL);
	assert(completed_tk2 != NULL);

	while (!TAILQ_EMPTY(&completed_tk2->elts)) {
		elt = TAILQ_FIRST(&completed_tk2->elts);
		TAILQ_REMOVE(&completed_tk2->elts, elt, next);
		ec_completed_tk_add_elt(completed_tk1, elt);
	}

	ec_completed_tk_free(completed_tk2);
}

void ec_completed_tk_free(struct ec_completed_tk *completed_tk)
{
	struct ec_completed_tk_elt *elt;

	if (completed_tk == NULL)
		return;

	while (!TAILQ_EMPTY(&completed_tk->elts)) {
		elt = TAILQ_FIRST(&completed_tk->elts);
		TAILQ_REMOVE(&completed_tk->elts, elt, next);
		ec_completed_tk_elt_free(elt);
	}
	ec_free(completed_tk->smallest_start);
	ec_free(completed_tk);
}

void ec_completed_tk_dump(FILE *out, const struct ec_completed_tk *completed_tk)
{
	struct ec_completed_tk_elt *elt;

	if (completed_tk == NULL || completed_tk->count == 0) {
		fprintf(out, "no completion\n");
		return;
	}

	fprintf(out, "completion: count=%u match=%u smallest_start=<%s>\n",
		completed_tk->count, completed_tk->count_match,
		completed_tk->smallest_start);

	TAILQ_FOREACH(elt, &completed_tk->elts, next) {
		fprintf(out, "add=<%s>, tk=%p, tk_type=%s\n",
			elt->add, elt->tk, elt->tk->ops->typename);
	}
}

const char *ec_completed_tk_smallest_start(
	const struct ec_completed_tk *completed_tk)
{
	if (completed_tk == NULL || completed_tk->smallest_start == NULL)
		return "";

	return completed_tk->smallest_start;
}

unsigned int ec_completed_tk_count(
	const struct ec_completed_tk *completed_tk,
	enum ec_completed_tk_filter_flags flags)
{
	unsigned int count = 0;

	if (completed_tk == NULL)
		return count;

	if (flags & EC_MATCH)
		count += completed_tk->count_match;
	if (flags & EC_NO_MATCH)
		count += (completed_tk->count - completed_tk->count_match); //XXX

	return count;
}

struct ec_completed_tk_iter *
ec_completed_tk_iter_new(struct ec_completed_tk *completed_tk,
	enum ec_completed_tk_filter_flags flags)
{
	struct ec_completed_tk_iter *iter;

	iter = ec_calloc(1, sizeof(*iter));
	if (iter == NULL)
		return NULL;

	iter->completed_tk = completed_tk;
	iter->flags = flags;
	iter->cur = NULL;

	return iter;
}

const struct ec_completed_tk_elt *ec_completed_tk_iter_next(
	struct ec_completed_tk_iter *iter)
{
	if (iter->completed_tk == NULL)
		return NULL;

	do {
		if (iter->cur == NULL) {
			iter->cur = TAILQ_FIRST(&iter->completed_tk->elts);
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

void ec_completed_tk_iter_free(struct ec_completed_tk_iter *iter)
{
	ec_free(iter);
}

const char *ec_tk_desc(const struct ec_tk *tk)
{
	if (tk->ops->desc != NULL)
		return tk->ops->desc(tk);

	return tk->desc;
}
