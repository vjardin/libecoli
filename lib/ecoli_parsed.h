/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

/**
 * Node parse API.
 *
 * The parse operation is to check if an input (a string or vector of
 * strings) matches the node tree. On success, the result is stored in a
 * tree that describes which part of the input matches which node.
 */

#ifndef ECOLI_PARSED_
#define ECOLI_PARSED_

#include <sys/queue.h>
#include <sys/types.h>
#include <limits.h>
#include <stdio.h>

struct ec_node;
struct ec_parsed;

/**
 * Create an empty parse tree.
 *
 * @return
 *   The empty parse tree.
 */
struct ec_parsed *ec_parsed(const struct ec_node *node);

/**
 *
 *
 *
 */
void ec_parsed_free(struct ec_parsed *parsed);

/**
 *
 *
 *
 */
void ec_parsed_free_children(struct ec_parsed *parsed);

/**
 *
 *
 *
 */
struct ec_parsed *ec_parsed_dup(const struct ec_parsed *parsed);

/**
 *
 *
 *
 */
const struct ec_strvec *ec_parsed_strvec(const struct ec_parsed *parsed);

/* a NULL return value is an error, with errno set
  ENOTSUP: no ->parse() operation
*/
/**
 *
 *
 *
 */
struct ec_parsed *ec_node_parse(const struct ec_node *node, const char *str);

/**
 *
 *
 *
 */
struct ec_parsed *ec_node_parse_strvec(const struct ec_node *node,
				const struct ec_strvec *strvec);

/**
 *
 *
 *
 */
#define EC_PARSED_NOMATCH INT_MAX

/* internal: used by nodes
 *
 * state is the current parse tree, which is built piece by piece while
 *   parsing the node tree: ec_node_parse_child() creates a new child in
 *   this state parse tree, and calls the parse() method for the child
 *   node, with state pointing to this new child. If it does not match,
 *   the child is removed in the state, else it is kept, with its
 *   possible descendants.
 *
 * return:
 * EC_PARSED_NOMATCH (positive) if it does not match
 * any other negative value (-errno) for other errors
 * the number of matched strings in strvec
 */
int ec_node_parse_child(const struct ec_node *node,
			struct ec_parsed *state,
			const struct ec_strvec *strvec);

/**
 *
 *
 *
 */
void ec_parsed_link_child(struct ec_parsed *parsed,
			struct ec_parsed *child);
/**
 *
 *
 *
 */
void ec_parsed_unlink_child(struct ec_parsed *parsed,
			struct ec_parsed *child);

/* keep the const */
#define ec_parsed_get_root(parsed) ({				\
	const struct ec_parsed *p_ = parsed; /* check type */	\
	struct ec_parsed *parsed_ = (struct ec_parsed *)parsed;	\
	typeof(parsed) res_;					\
	(void)p_;						\
	res_ = __ec_parsed_get_root(parsed_);			\
	res_;							\
})

/**
 *
 *
 *
 */
struct ec_parsed *__ec_parsed_get_root(struct ec_parsed *parsed);

/**
 *
 *
 *
 */
struct ec_parsed *ec_parsed_get_parent(const struct ec_parsed *parsed);

/**
 * Get the first child of a tree.
 *
 */
struct ec_parsed *ec_parsed_get_first_child(const struct ec_parsed *parsed);

/**
 *
 *
 *
 */
struct ec_parsed *ec_parsed_get_last_child(const struct ec_parsed *parsed);

/**
 *
 *
 *
 */
struct ec_parsed *ec_parsed_get_next(const struct ec_parsed *parsed);

/**
 *
 *
 *
 */
#define EC_PARSED_FOREACH_CHILD(child, parsed)			\
	for (child = ec_parsed_get_first_child(parsed);		\
		child != NULL;					\
		child = ec_parsed_get_next(child))		\

/**
 *
 *
 *
 */
bool ec_parsed_has_child(const struct ec_parsed *parsed);

/**
 *
 *
 *
 */
const struct ec_node *ec_parsed_get_node(const struct ec_parsed *parsed);

/**
 *
 *
 *
 */
void ec_parsed_del_last_child(struct ec_parsed *parsed);

/**
 *
 *
 *
 */
struct ec_keyval *ec_parsed_get_attrs(struct ec_parsed *parsed);

/**
 *
 *
 *
 */
void ec_parsed_dump(FILE *out, const struct ec_parsed *parsed);

/**
 *
 *
 *
 */
struct ec_parsed *ec_parsed_find_first(struct ec_parsed *parsed,
	const char *id);

/**
 * Iterate among parsed tree
 *
 * Use it with:
 * for (iter = state; iter != NULL; iter = ec_parsed_iter_next(iter))
 */
struct ec_parsed *ec_parsed_iter_next(struct ec_parsed *parsed);

/**
 *
 *
 *
 */
size_t ec_parsed_len(const struct ec_parsed *parsed);

/**
 *
 *
 *
 */
size_t ec_parsed_matches(const struct ec_parsed *parsed);

#endif
