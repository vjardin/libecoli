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

#include <ecoli_malloc.h>
#include <ecoli_tk.h>

struct ec_tk *ec_tk_new(const char *id, const struct ec_tk_ops *ops,
	size_t size)
{
	struct ec_tk *tk = NULL;

	assert(size >= sizeof(*tk));

	tk = ec_calloc(1, size);
	if (tk == NULL)
		goto fail;

	if (id != NULL) {
		tk->id = ec_strdup(id);
		if (tk->id == NULL)
			goto fail;
	}

	tk->ops = ops;

	return tk;

 fail:
	ec_tk_free(tk);
	return NULL;
}

void ec_tk_free(struct ec_tk *tk)
{
	if (tk == NULL)
		return;

	if (tk->ops != NULL && tk->ops->free_priv != NULL)
		tk->ops->free_priv(tk);
	ec_free(tk->id);
	ec_free(tk);
}

struct ec_parsed_tk *ec_tk_parse(const struct ec_tk *token, const char *str)
{
	struct ec_parsed_tk *parsed_tk;

	parsed_tk = token->ops->parse(token, str);

	return parsed_tk;
}


struct ec_parsed_tk *ec_parsed_tk_new(const struct ec_tk *tk)
{
	struct ec_parsed_tk *parsed_tk;

	parsed_tk = ec_calloc(1, sizeof(*parsed_tk));
	if (parsed_tk == NULL)
		goto fail;

	parsed_tk->tk = tk;
	TAILQ_INIT(&parsed_tk->children);

	return parsed_tk;

 fail:
	return NULL;
}

void ec_parsed_tk_free(struct ec_parsed_tk *parsed_tk)
{
	struct ec_parsed_tk *child;

	if (parsed_tk == NULL)
		return;

	while (!TAILQ_EMPTY(&parsed_tk->children)) {
		child = TAILQ_FIRST(&parsed_tk->children);
		TAILQ_REMOVE(&parsed_tk->children, child, next);
		ec_parsed_tk_free(child);
	}
	ec_free(parsed_tk->str);
	ec_free(parsed_tk);
}

static void __ec_parsed_tk_dump(FILE *out, const struct ec_parsed_tk *parsed_tk,
	size_t indent)
{
	struct ec_parsed_tk *child;
	size_t i;
	const char *s;

	/* XXX enhance */
	for (i = 0; i < indent; i++)
		fprintf(out, " ");
	s = ec_parsed_tk_to_string(parsed_tk);
	fprintf(out, "id=%s, s=<%s>\n", parsed_tk->tk->id, s);

	TAILQ_FOREACH(child, &parsed_tk->children, next)
		__ec_parsed_tk_dump(out, child, indent + 2);
}

void ec_parsed_tk_dump(FILE *out, const struct ec_parsed_tk *parsed_tk)
{
	if (parsed_tk == NULL) {
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

	if (parsed_tk->tk->id != NULL && !strcmp(parsed_tk->tk->id, id))
		return parsed_tk;

	TAILQ_FOREACH(child, &parsed_tk->children, next) {
		ret = ec_parsed_tk_find_first(child, id);
		if (ret != NULL)
			return ret;
	}

	return NULL;
}

const char *ec_parsed_tk_to_string(const struct ec_parsed_tk *parsed_tk)
{
	if (parsed_tk == NULL)
		return NULL;

	return parsed_tk->str;
}

struct ec_completed_tk *ec_tk_complete(const struct ec_tk *token,
	const char *str)
{
	struct ec_completed_tk *completed_tk;

	completed_tk = token->ops->complete(token, str);

	return completed_tk;
}

struct ec_completed_tk *ec_completed_tk_new(void)
{
	struct ec_completed_tk *completed_tk = NULL;

	completed_tk = ec_calloc(1, sizeof(*completed_tk));
	if (completed_tk == NULL)
		return NULL;

	TAILQ_INIT(&completed_tk->elts);
	completed_tk->count = 0;

	return completed_tk;
}

struct ec_completed_tk_elt *ec_completed_tk_elt_new(const struct ec_tk *tk,
	const char *add, const char *full)
{
	struct ec_completed_tk_elt *elt = NULL;

	elt = ec_calloc(1, sizeof(*elt));
	if (elt == NULL)
		return NULL;

	elt->tk = tk;
	elt->add = ec_strdup(add);
	elt->full = ec_strdup(full);
	if (elt->add == NULL || elt->full == NULL) {
		ec_completed_tk_elt_free(elt);
		return NULL;
	}

	return elt;
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
	ec_free(elt->full);
	ec_free(elt);
}

struct ec_completed_tk *ec_completed_tk_merge(
	struct ec_completed_tk *completed_tk1,
	struct ec_completed_tk *completed_tk2)
{
	struct ec_completed_tk_elt *elt;

	if (completed_tk2 == NULL)
		return completed_tk1;

	if (completed_tk1 == NULL)
		return completed_tk2;

	while (!TAILQ_EMPTY(&completed_tk2->elts)) {
		elt = TAILQ_FIRST(&completed_tk2->elts);
		TAILQ_REMOVE(&completed_tk2->elts, elt, next);
		ec_completed_tk_add_elt(completed_tk1, elt);
	}

	ec_completed_tk_free(completed_tk2);

	return completed_tk1;
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

	fprintf(out, "completion: count=%u smallest_start=<%s>\n",
		completed_tk->count, completed_tk->smallest_start);

	TAILQ_FOREACH(elt, &completed_tk->elts, next)
		fprintf(out, "add=<%s> full=<%s>\n", elt->add, elt->full);
}

const char *ec_completed_tk_smallest_start(
	const struct ec_completed_tk *completed_tk)
{
	if (completed_tk == NULL)
		return NULL;

	return completed_tk->smallest_start;
}

unsigned int ec_completed_tk_count(const struct ec_completed_tk *completed_tk)
{
	struct ec_completed_tk_elt *elt;
	unsigned int count = 0;

	if (completed_tk == NULL)
		return 0;

	TAILQ_FOREACH(elt, &completed_tk->elts, next)
		count++;

	return count;
}
