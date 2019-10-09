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
 * XXX comp vs item
 *
 * @}
 */

#ifndef ECOLI_COMPLETE_
#define ECOLI_COMPLETE_

#include <sys/queue.h>
#include <sys/types.h>
#include <stdio.h>

struct ec_node;

enum ec_comp_type { /* XXX should be a define */
	EC_COMP_UNKNOWN = 0x1,
	EC_COMP_FULL = 0x2,
	EC_COMP_PARTIAL = 0x4,
	EC_COMP_ALL = 0x7,
};

struct ec_comp_item;

TAILQ_HEAD(ec_comp_item_list, ec_comp_item);

struct ec_comp_group {
	TAILQ_ENTRY(ec_comp_group) next;
	const struct ec_node *node;
	struct ec_comp_item_list items;
	struct ec_parse *state;
	struct ec_dict *attrs;
};

TAILQ_HEAD(ec_comp_group_list, ec_comp_group);

struct ec_comp {
	unsigned count;
	unsigned count_full;
	unsigned count_partial;
	unsigned count_unknown;
	struct ec_parse *cur_state;
	struct ec_comp_group *cur_group;
	struct ec_comp_group_list groups;
	struct ec_dict *attrs;
};

/*
 * return a comp object filled with items
 * return NULL on error (nomem?)
 */
struct ec_comp *ec_node_complete(const struct ec_node *node,
	const char *str);
struct ec_comp *ec_node_complete_strvec(const struct ec_node *node,
	const struct ec_strvec *strvec);

/* internal: used by nodes */
int ec_node_complete_child(const struct ec_node *node,
			struct ec_comp *comp,
			const struct ec_strvec *strvec);

/**
 * Create a completion object (list of completion items).
 *
 *
 */
struct ec_comp *ec_comp(struct ec_parse *state);

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

struct ec_parse *ec_comp_get_state(struct ec_comp *comp);

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
 *
 *
 *
 */
int
ec_node_complete_unknown(const struct ec_node *gen_node,
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
struct ec_comp_iter {
	enum ec_comp_type type;
	const struct ec_comp *comp;
	struct ec_comp_group *cur_node;
	struct ec_comp_item *cur_match;
};

/**
 *
 *
 *
 */
struct ec_comp_iter *
ec_comp_iter(const struct ec_comp *comp,
	enum ec_comp_type type);

/**
 *
 *
 *
 */
struct ec_comp_item *ec_comp_iter_next(
	struct ec_comp_iter *iter);

/**
 *
 *
 *
 */
void ec_comp_iter_free(struct ec_comp_iter *iter);


#endif
