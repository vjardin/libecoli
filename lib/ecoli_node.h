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

/**
 * Interface to manage the ecoli nodes.
 *
 * A node is a main structure of the ecoli library, used to define how
 * to match and complete the input tokens. A node is a generic object
 * that implements:
 * - a parse(node, input) method: check if an input matches
 * - a complete(node, input) method: return possible completions for
 *   a given input
 * - some other methods to initialize, free, ...
 *
 * One basic example is the string node (ec_node_str). A node
 * ec_node_str("foo") will match any token list starting with "foo",
 * for example:
 * - ["foo"]
 * - ["foo", "bar", ...]
 * But will not match:
 * - []
 * - ["bar", ...]
 *
 * A node ec_node_str("foo") will complete with "foo" if the input
 * contains one token, with the same beginning than "foo":
 * - [""]
 * - ["f"]
 * - ["fo"]
 * - ["foo"]
 * But it will not complete:
 * - []
 * - ["bar"]
 * - ["f", ""]
 * - ["", "f"]
 *
 * A node can have child nodes. For instance, a sequence node
 * ec_node_seq(ec_node_str("foo"), ec_node_str("bar")) will match
 * ["foo", "bar"].
 */

#ifndef ECOLI_NODE_
#define ECOLI_NODE_

#include <sys/queue.h>
#include <sys/types.h>
#include <stdio.h>

#define EC_NODE_ENDLIST ((void *)1)

struct ec_node;
struct ec_parsed;
struct ec_completed;
struct ec_strvec;
struct ec_keyval;

#define EC_NODE_TYPE_REGISTER(t)						\
	static void ec_node_init_##t(void);				\
	static void __attribute__((constructor, used))			\
	ec_node_init_##t(void)						\
	{								\
		if (ec_node_type_register(&t) < 0)			\
			fprintf(stderr, "cannot register %s\n", t.name); \
	}

TAILQ_HEAD(ec_node_type_list, ec_node_type);

/* return 0 on success, else -errno. */
typedef int (*ec_node_build_t)(struct ec_node *node);

typedef int (*ec_node_parse_t)(const struct ec_node *node,
			struct ec_parsed *state,
			const struct ec_strvec *strvec);
typedef int (*ec_node_complete_t)(const struct ec_node *node,
				struct ec_completed *completed_state,
				struct ec_parsed *parsed_state,
				const struct ec_strvec *strvec);
typedef size_t (*ec_node_get_max_parse_len_t)(const struct ec_node *node);
typedef const char * (*ec_node_desc_t)(const struct ec_node *);
typedef void (*ec_node_init_priv_t)(struct ec_node *);
typedef void (*ec_node_free_priv_t)(struct ec_node *);

/**
 * A structure describing a node type.
 */
struct ec_node_type {
	TAILQ_ENTRY(ec_node_type) next;  /**< Next in list. */
	const char *name;              /**< Node type name. */
	ec_node_build_t build; /* (re)build the node, called by generic parse */
	ec_node_parse_t parse;
	ec_node_complete_t complete;
	ec_node_get_max_parse_len_t get_max_parse_len;
	ec_node_desc_t desc;
	size_t size;
	ec_node_init_priv_t init_priv;
	ec_node_free_priv_t free_priv;
};

/**
 * Register a node type.
 *
 * @param type
 *   A pointer to a ec_test structure describing the test
 *   to be registered.
 * @return
 *   0 on success, negative value on error.
 */
int ec_node_type_register(struct ec_node_type *type);

/**
 * Lookup node type by name
 *
 * @param name
 *   The name of the node type to search.
 * @return
 *   The node type if found, or NULL on error.
 */
struct ec_node_type *ec_node_type_lookup(const char *name);

/**
 * Dump registered log types
 */
void ec_node_type_dump(FILE *out);

TAILQ_HEAD(ec_node_list, ec_node);

struct ec_node {
	const struct ec_node_type *type;
	char *id;
	char *desc;
	struct ec_keyval *attrs;
	/* XXX ensure parent and child are properly set in all nodes */
	struct ec_node *parent;
	unsigned int refcnt;
#define EC_NODE_F_BUILT 0x0001 /** set if configuration is built */
	unsigned int flags;

	TAILQ_ENTRY(ec_node) next;
	struct ec_node_list children;
};

/* create a new node when the type is known, typically called from the node
 * code */
struct ec_node *__ec_node(const struct ec_node_type *type, const char *id);

/* create a_new node node */
struct ec_node *ec_node(const char *typename, const char *id);

struct ec_node *ec_node_clone(struct ec_node *node);
void ec_node_free(struct ec_node *node);

/**
 * Get the max len of strvec that can be parsed by this node
 *
 * If there is no maximum, return SIZE_MAX.
 */
size_t ec_node_get_max_parse_len(const struct ec_node *node);

/* XXX add more accessors */
struct ec_keyval *ec_node_attrs(const struct ec_node *node);
struct ec_node *ec_node_parent(const struct ec_node *node);
const char *ec_node_id(const struct ec_node *node);
const char *ec_node_desc(const struct ec_node *node);

void ec_node_dump(FILE *out, const struct ec_node *node);
struct ec_node *ec_node_find(struct ec_node *node, const char *id);

#endif
