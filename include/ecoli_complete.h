/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

/**
 * @defgroup complete Complete
 * @{
 *
 * @brief Complete string input using a grammar tree
 *
 * This file provide helpers to list and manipulate the possible
 * completions for a given input.
 *
 * Use @ec_complete_strvec() to complete a vector of strings when
 * the input is already split into several tokens. You can use
 * @ec_complete() if you know that the size of the vector is
 * 1. This is common if you grammar tree has a lexer that will tokenize
 * the input.
 *
 * These 2 functions return a pointer to an @ec_comp structure, that
 * lists the possible completions. The completions are grouped into
 * @ec_group. All completions items of a group shares the same parsing
 * state and are issued by the same node.
 *
 * @}
 */

#ifndef ECOLI_COMPLETE_
#define ECOLI_COMPLETE_

#include <sys/queue.h>
#include <sys/types.h>
#include <stdio.h>

struct ec_node;
struct ec_comp_item;
struct ec_comp_group;
struct ec_comp;

/**
 * Completion item type.
 */
enum ec_comp_type {
	EC_COMP_UNKNOWN = 0x1,
	EC_COMP_FULL = 0x2,
	EC_COMP_PARTIAL = 0x4,
	EC_COMP_ALL = 0x7,
};

/**
 * Get the list of completions from a string input.
 *
 * It is equivalent that calling @ec_complete_strvec() with a
 * vector that only contains 1 element, the input string. Using this
 * function is often more convenient if you get your input from a
 * buffer, because you won't have to create a vector. Usually, it means
 * you have a lexer in your grammar tree that will tokenize the input.
 *
 * See @ec_complete_strvec() for more details.
 *
 * @param node
 *   The grammar tree.
 * @param str
 *   The input string.
 * @return
 *   A pointer to the completion list on success, or NULL
 *   on error (errno is set).
 */
struct ec_comp *ec_complete(const struct ec_node *node,
	const char *str);

/**
 * Get the list of completions from a string vector input.
 *
 * This function tries to complete the last element of the given string
 * vector. For instance, to complete with file names in an equivalent of
 * the "cat" shell command, the passed vector should be ["cat", ""] (and
 * not ["cat"]). To complete with files starting with "x", the passed
 * vector should be ["cat", "x"].
 *
 * To get the completion list, the engine parses the beginning of the
 * input using the grammar tree. The resulting parsing tree is saved and
 * attached to each completion group.
 *
 * The result is a @ec_comp structure pointer, which contains several
 * groups of completion items.
 *
 * @param node
 *   The grammar tree.
 * @param str
 *   The input string.
 * @return
 *   A pointer to the completion list on success, or NULL
 *   on error (errno is set).
 */
struct ec_comp *ec_complete_strvec(const struct ec_node *node,
	const struct ec_strvec *strvec);

/**
 * Get the list of completions when called from a node completion.
 *
 * This function is to be used by ecoli nodes.
 *
 */
int ec_complete_child(const struct ec_node *node,
			struct ec_comp *comp,
			const struct ec_strvec *strvec);

/**
 * Create a completion object (list of completion items).
 *
 *
 */
struct ec_comp *ec_comp(struct ec_pnode *state);

/**
 * Free a completion object and all its items.
 *
 *
 */
void ec_comp_free(struct ec_comp *comp);

/**
 *
 *
 *
 */
void ec_comp_dump(FILE *out,
	const struct ec_comp *comp);

/**
 * Merge items contained in 'from' into 'to'
 *
 * The 'from' comp struct is freed.
 */
int ec_comp_merge(struct ec_comp *to,
		struct ec_comp *from);

/**
 * Get current completion state.
 *
 */
struct ec_pnode *ec_comp_get_state(const struct ec_comp *comp);

/**
 * Get current completion group.
 *
 */
struct ec_comp_group *ec_comp_get_group(const struct ec_comp *comp);

/**
 * Get completion group attributes.
 *
 */
struct ec_dict *ec_comp_get_attrs(const struct ec_comp *comp);

/* shortcut for ec_comp_item() + ec_comp_item_add() */
int ec_comp_add_item(struct ec_comp *comp,
			const struct ec_node *node,
			struct ec_comp_item **p_item,
			enum ec_comp_type type,
			const char *start, const char *full);

/**
 *
 */
int ec_comp_item_set_str(struct ec_comp_item *item,
			const char *str);

/**
 * Get the string value of a completion item.
 *
 *
 */
const char *
ec_comp_item_get_str(const struct ec_comp_item *item);

/**
 * Get the display string value of a completion item.
 *
 *
 */
const char *
ec_comp_item_get_display(const struct ec_comp_item *item);

/**
 * Get the completion string value of a completion item.
 *
 *
 */
const char *
ec_comp_item_get_completion(const struct ec_comp_item *item);

/**
 * Get the group of a completion item.
 *
 *
 */
const struct ec_comp_group *
ec_comp_item_get_grp(const struct ec_comp_item *item);

/**
 * Get the type of a completion item.
 *
 *
 */
enum ec_comp_type
ec_comp_item_get_type(const struct ec_comp_item *item);

/**
 * Get the node associated to a completion item.
 *
 *
 */
const struct ec_node *
ec_comp_item_get_node(const struct ec_comp_item *item);

/**
 * Set the display value of an item.
 *
 *
 */
int ec_comp_item_set_display(struct ec_comp_item *item,
				const char *display);

/**
 * Set the completion value of an item.
 *
 *
 */
int ec_comp_item_set_completion(struct ec_comp_item *item,
				const char *completion);

/**
 * Get the completion group node.
 *
 *
 */
const struct ec_node *
ec_comp_group_get_node(const struct ec_comp_group *grp);

/**
 * Get the completion group parsed state.
 *
 *
 */
const struct ec_pnode *
ec_comp_group_get_state(const struct ec_comp_group *grp);

/**
 * Get the completion group attributes.
 *
 *
 */
const struct ec_dict *
ec_comp_group_get_attrs(const struct ec_comp_group *grp);


/**
 *
 *
 *
 */
int
ec_complete_unknown(const struct ec_node *gen_node,
			struct ec_comp *comp,
			const struct ec_strvec *strvec);

/**
 *
 *
 *
 */
unsigned int ec_comp_count(
	const struct ec_comp *comp,
	enum ec_comp_type flags);

/**
 *
 *
 *
 */

/**
 * Get the first completion item matching the type.
 *
 * Also see EC_COMP_FOREACH().
 *
 * @param comp
 *   The completion list.
 * @param type
 *   A logical OR of flags among EC_COMP_UNKNOWN, EC_COMP_PARTIAL and
 *   EC_COMP_FULL, to select the type to iterate.
 * @return
 *   A completion item.
 */
struct ec_comp_item *
ec_comp_iter_first(const struct ec_comp *comp, enum ec_comp_type type);

/**
 * Get the first completion item matching the type.
 *
 * Also see EC_COMP_FOREACH().
 *
 * @param comp
 *   The completion list.
 * @param type
 *   A logical OR of flags among EC_COMP_UNKNOWN, EC_COMP_PARTIAL and
 *   EC_COMP_FULL, to select the type to iterate.
 * @return
 *   A completion item.
 */
struct ec_comp_item *
ec_comp_iter_next(struct ec_comp_item *item, enum ec_comp_type type);

/**
 * Iterate items matching a given type.
 *
 * @param item
 *   The item that will be set at each iteration.
 * @param comp
 *   The completion list.
 * @param type
 *   A logical OR of flags among EC_COMP_UNKNOWN, EC_COMP_PARTIAL and
 *   EC_COMP_FULL, to select the type to iterate.
 */
#define EC_COMP_FOREACH(item, comp, type)		\
	for (item = ec_comp_iter_first(comp, type);	\
	     item != NULL;				\
	     item = ec_comp_iter_next(item, type))

#endif
