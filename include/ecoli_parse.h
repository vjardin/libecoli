/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

/**
 * @defgroup parse Parse
 * @{
 *
 * @brief Create parse tree from string input and grammar graph
 *
 * Node parse API.
 *
 * The parse operation is to check if an input (a string or vector of
 * strings) matches the node tree. On success, the result is stored in a
 * tree that describes which part of the input matches which node.
 *
 * The parsing tree is sometimes referenced by another node than the
 * root node. Use ec_pnode_get_root() to get the root node in that case.
 * @}
 */

#ifndef ECOLI_PNODE_
#define ECOLI_PNODE_

#include <sys/queue.h>
#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <stdbool.h>

struct ec_node;
struct ec_pnode;

/**
 * Create an empty parsing tree.
 *
 * This function is used internally when parsing an input using
 * a grammar tree.
 *
 * @param node
 *   The grammar node.
 * @return
 *   The empty parse tree.
 */
struct ec_pnode *ec_pnode(const struct ec_node *node);

/**
 * Free a parsing tree.
 *
 * @param pnode
 *   The root of the parsing tree to be freed. It must not have
 *   any parent.
 */
void ec_pnode_free(struct ec_pnode *pnode);

/**
 * Remove and free all the children of a parsing tree node.
 *
 * @param pnode
 *   Node whose children will be freed.
 */
void ec_pnode_free_children(struct ec_pnode *pnode);

/**
 * Duplicate a parsing tree.
 *
 * @param pnode
 *   A node inside a parsing tree.
 * @return
 *   A pointer to the copy of the input node, at the same place in the
 *   copy of the parsing tree. Return NULL on error (errno is set).
 */
struct ec_pnode *ec_pnode_dup(const struct ec_pnode *pnode);

/**
 * Get the string vector associated to a parsing node.
 *
 * When an input is parsed successfully (i.e. the input string vector
 * matches the grammar tree), the matching string vector is copied
 * inside the associated parsing node.
 *
 * For instance, parsing the input ["foo", "bar"] on a grammar which is
 * a sequence of strings, the attached string vector will be ["foo",
 * "bar"] on the root pnode, ["foo"] on the first leaf, and ["bar"] on
 * the second leaf.
 *
 * If the parsing tree does not match (see ec_pnode_matches()), it
 * the associated string vector is NULL.
 *
 * @param pnode
 *   The parsing node. If NULL, the function returns NULL.
 * @return
 *   The string vector associated to the parsing node, or NULL
 *   if the node is not yet parsed (this happens when building the
 *   parsing tree), or if the parsing tree does not match the
 *   input.
 */
const struct ec_strvec *ec_pnode_get_strvec(const struct ec_pnode *pnode);

/**
 * Parse a string using a grammar tree.
 *
 * This is equivalent to calling ec_parse_strvec() on the same
 * node, with a string vector containing only the argument string str.
 *
 * @param node
 *   The grammar node.
 * @param str
 *   The input string.
 * @return
 *   A parsing tree, or NULL on error (errno is set).
 */
struct ec_pnode *ec_parse(const struct ec_node *node, const char *str);

/**
 * Parse a string vector using a grammar tree.
 *
 * Generate a parsing tree by parsing the input string vector using the
 * given grammar tree.
 *
 * The parsing tree is composed of struct ec_pnode, and each of them is
 * associated to a struct ec_node (the grammar node), to the string vector
 * that matched the subtree, and to optional attributes.
 *
 * When the input matches the grammar tree, the string vector associated
 * to the root node of the returned parsing tree is the same than the
 * strvec argument. Calling ec_pnode_matches() on this tree returns true.
 *
 * If the input does not match the grammar tree, the returned parsing
 * tree only contains one root node, with no associated string vector.
 * Calling ec_pnode_matches() on this tree returns false.
 *
 * @param node
 *   The grammar node.
 * @param str
 *   The input string vector.
 * @return
 *   A parsing tree, or NULL on error (errno is set).
 */
struct ec_pnode *ec_parse_strvec(const struct ec_node *node,
				const struct ec_strvec *strvec);

/**
 *
 *
 *
 */
#define EC_PARSE_NOMATCH INT_MAX

/* internal: used by nodes
 *
 * pstate is the current parse tree, which is built piece by piece while
 *   parsing the node tree: ec_parse_child() creates a new child in
 *   this state parse tree, and calls the parse() method for the child
 *   node, with pstate pointing to this new child. If it does not match,
 *   the child is removed in the state, else it is kept, with its
 *   possible descendants.
 *
 * return:
 * the number of matched strings in strvec on success
 * EC_PARSE_NOMATCH (positive) if it does not match
 * -1 on error, and errno is set
 */
int ec_parse_child(const struct ec_node *node,
			struct ec_pnode *pstate,
			const struct ec_strvec *strvec);

/**
 *
 *
 *
 */
void ec_pnode_link_child(struct ec_pnode *pnode,
			struct ec_pnode *child);
/**
 *
 *
 *
 */
void ec_pnode_unlink_child(struct ec_pnode *pnode,
			struct ec_pnode *child);

/* keep the const */
#define ec_pnode_get_root(parse) ({				\
	const struct ec_pnode *p_ = parse; /* check type */	\
	struct ec_pnode *pnode_ = (struct ec_pnode *)parse;	\
	typeof(parse) res_;					\
	(void)p_;						\
	res_ = __ec_pnode_get_root(pnode_);			\
	res_;							\
})

/**
 *
 *
 *
 */
struct ec_pnode *__ec_pnode_get_root(struct ec_pnode *pnode);

/**
 *
 *
 *
 */
struct ec_pnode *ec_pnode_get_parent(const struct ec_pnode *pnode);

/**
 * Get the first child of a tree.
 *
 */
struct ec_pnode *ec_pnode_get_first_child(const struct ec_pnode *pnode);

/**
 *
 *
 *
 */
struct ec_pnode *ec_pnode_get_last_child(const struct ec_pnode *pnode);

/**
 *
 *
 *
 */
struct ec_pnode *ec_pnode_next(const struct ec_pnode *pnode);

/**
 *
 *
 *
 */
#define EC_PNODE_FOREACH_CHILD(child, parse)			\
	for (child = ec_pnode_get_first_child(parse);		\
	     child != NULL;					\
	     child = ec_pnode_next(child))			\

/**
 *
 *
 *
 */
bool ec_pnode_has_child(const struct ec_pnode *pnode);

/**
 *
 *
 *
 */
const struct ec_node *ec_pnode_get_node(const struct ec_pnode *pnode);

/**
 *
 *
 *
 */
void ec_pnode_del_last_child(struct ec_pnode *pnode);

/**
 *
 *
 *
 */
struct ec_dict *ec_pnode_get_attrs(struct ec_pnode *pnode);

/**
 *
 *
 *
 */
void ec_pnode_dump(FILE *out, const struct ec_pnode *pnode);

/**
 *
 *
 *
 */
struct ec_pnode *ec_pnode_find(struct ec_pnode *pnode,
	const char *id);

/**
 *
 *
 *
 */
struct ec_pnode *ec_pnode_find_next(struct ec_pnode *root,
				struct ec_pnode *start,
				const char *id, bool iter_children);

/**
 * Iterate among parse tree
 *
 * Use it with:
 * for (iter = pnode; iter != NULL; iter = EC_PNODE_ITER_NEXT(pnode, iter, 1))
 */
struct ec_pnode *__ec_pnode_iter_next(const struct ec_pnode *root,
				struct ec_pnode *pnode, bool iter_children);

/* keep the const if any */
#define EC_PNODE_ITER_NEXT(root, parse, iter_children) ({		\
	const struct ec_pnode *p_ = parse; /* check type */		\
	struct ec_pnode *pnode_ = (struct ec_pnode *)parse;		\
	typeof(parse) res_;						\
	(void)p_;							\
	res_ = __ec_pnode_iter_next(root, pnode_, iter_children);	\
	res_;								\
})

/**
 *
 *
 *
 */
size_t ec_pnode_len(const struct ec_pnode *pnode);

/**
 *
 *
 *
 */
size_t ec_pnode_matches(const struct ec_pnode *pnode);

#endif
