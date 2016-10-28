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

#ifndef ECOLI_TK_
#define ECOLI_TK_

#include <sys/queue.h>
#include <sys/types.h>

#include <stdio.h>

#define EC_TK_ENDLIST ((void *)1)

struct ec_tk;
struct ec_parsed_tk;

typedef struct ec_parsed_tk *(*ec_tk_parse_t)(const struct ec_tk *tk,
	const char *str);
typedef struct ec_completed_tk *(*ec_tk_complete_t)(const struct ec_tk *tk,
	const char *str);
typedef void (*ec_tk_free_priv_t)(struct ec_tk *);

struct ec_tk_ops {
	ec_tk_parse_t parse;
	ec_tk_complete_t complete;
	ec_tk_free_priv_t free_priv;
};

struct ec_tk {
	const struct ec_tk_ops *ops;
	char *id;
};

struct ec_tk *ec_tk_new(const char *id, const struct ec_tk_ops *ops,
	size_t priv_size);
void ec_tk_free(struct ec_tk *tk);


TAILQ_HEAD(ec_parsed_tk_list, ec_parsed_tk);

struct ec_parsed_tk {
	struct ec_parsed_tk_list children;
	TAILQ_ENTRY(ec_parsed_tk) next;
	const struct ec_tk *tk;
	char *str;
};

struct ec_parsed_tk *ec_parsed_tk_new(const struct ec_tk *tk);
struct ec_parsed_tk *ec_tk_parse(const struct ec_tk *token, const char *str);
void ec_parsed_tk_add_child(struct ec_parsed_tk *parsed_tk,
	struct ec_parsed_tk *child);
void ec_parsed_tk_dump(FILE *out, const struct ec_parsed_tk *parsed_tk);
void ec_parsed_tk_free(struct ec_parsed_tk *parsed_tk);

struct ec_parsed_tk *ec_parsed_tk_find_first(struct ec_parsed_tk *parsed_tk,
	const char *id);

const char *ec_parsed_tk_to_string(const struct ec_parsed_tk *parsed_tk);

struct ec_completed_tk_elt {
	TAILQ_ENTRY(ec_completed_tk_elt) next;
	const struct ec_tk *tk;
	char *add;
	char *full;
};

TAILQ_HEAD(ec_completed_tk_elt_list, ec_completed_tk_elt);


struct ec_completed_tk {
	struct ec_completed_tk_elt_list elts;
	unsigned count;
	char *smallest_start;
};

/*
 * return NULL if it does not match the beginning of the token
 * return "" if it matches but does not know how to complete
 * return "xyz" if it knows how to complete
 */
struct ec_completed_tk *ec_tk_complete(const struct ec_tk *token,
	const char *str);
struct ec_completed_tk *ec_completed_tk_new(void);
struct ec_completed_tk_elt *ec_completed_tk_elt_new(const struct ec_tk *tk,
	const char *add, const char *full);
void ec_completed_tk_add_elt(
	struct ec_completed_tk *completed_tk, struct ec_completed_tk_elt *elt);
void ec_completed_tk_elt_free(struct ec_completed_tk_elt *elt);
struct ec_completed_tk *ec_completed_tk_merge(
	struct ec_completed_tk *completed_tk1,
	struct ec_completed_tk *completed_tk2);
void ec_completed_tk_free(struct ec_completed_tk *completed_tk);
void ec_completed_tk_dump(FILE *out,
	const struct ec_completed_tk *completed_tk);

const char *ec_completed_tk_smallest_start(
	const struct ec_completed_tk *completed_tk);

unsigned int ec_completed_tk_count(const struct ec_completed_tk *completed_tk);

#endif
