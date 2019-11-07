/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

/**
 * @defgroup grammar_tree Grammar Tree
 * @{
 *
 * @brief Libecoli grammar nodes.
 *
 * The grammar node is a main structure of the ecoli library, used to define
 * how to match and complete the input tokens. A node is a generic object
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

/**
 * Node has no identifier.
 */
#define EC_NO_ID "no-id"

struct ec_node;
struct ec_pnode;
struct ec_comp;
struct ec_strvec;
struct ec_dict;
struct ec_config;
struct ec_config_schema;

/**
 * Register a node type at library load.
 *
 * The node type is registered in a function that has the the
 * constructor attribute: the function is called at library load.
 *
 * @param t
 *   The name of the ec_node_type structure variable.
 */
#define EC_NODE_TYPE_REGISTER(t)					\
	static void ec_node_init_##t(void);				\
	static void __attribute__((constructor, used))			\
	ec_node_init_##t(void)						\
	{								\
		if (ec_node_type_register(&t) < 0)			\
			fprintf(stderr,					\
				"cannot register node type %s\n",	\
				t.name);				\
	}

/**
 * A list of node types.
 */
TAILQ_HEAD(ec_node_type_list, ec_node_type);

/**
 * Function type used to configure a node.
 *
 * The function pointer is not called directly, the helper
 * @ec_node_set_config() should be used instead.
 *
 * @param node
 *   The node to configure.
 * @param config
 *   The configuration to apply to the node.
 * @return
 *   0 on success, negative on error (errno is set).
 */
typedef int (*ec_node_set_config_t)(struct ec_node *node,
				const struct ec_config *config);

/**
 * Parse a string vector using the given grammar tree.
 *
 * The function pointer is not called directly, the helpers @ec_parse(),
 * ec_parse_strvec() or ec_parse_child() should be used instead.
 *
 * The implementation of this method for a node that manages children
 * will call ec_parse_child(child, state, child_strvec).
 *
 * @param node
 *   The root node of the grammar tree.
 * @param state
 *   A pointer to the leaf being parsed in the parsing tree. It can be
 *   used by a node to retrieve information from the current parsing
 *   tree. To get the root of the tree, @ec_pnode_get_root(state) should
 *   be used.
 * @param strvec
 *   The string vector to be parsed.
 * @return
 *   On success, return the number of consumed items in the string vector
 *   (can be 0) or EC_PARSE_NOMATCH if the node cannot parse the string
 *   vector. On error, a negative value is returned and errno is set.
 */
typedef int (*ec_parse_t)(const struct ec_node *node,
			struct ec_pnode *state,
			const struct ec_strvec *strvec);

/**
 * Get completion items using the given grammar tree.
 *
 * The function pointer is not called directly, the helpers @ec_complete(),
 * ec_complete_strvec() or ec_complete_child() should be used instead.
 *
 * This function completes the last element of the string vector.
 * For instance, node.type->complete(node, comp, ["ls"]) will
 * list all commands that starts with "ls", while
 * node.type->complete(node, comp, ["ls", ""]) will list all
 * possible values for the next argument.

 * The implementation of this function is supposed to either:
 * - call @ec_comp_add_item(node, comp, ...) for each completion item
 *   that should be added to the list. This is done in terminal nodes,
 *   for example in ec_node_str or ec_node_file.
 * - call @ec_complete_child(child, comp, child_strvec) to let
 *   the children nodes add their own completion. This is the
 *   case of ec_node_or which trivially calls @ec_complete_child()
 *   on all its children, and of ec_node_seq, which has to
 *   do a more complex job (parsing strvec).
 *
 * A node that does not provide any method for the completion
 * will fallback to ec_complete_unknown(): this helper returns
 * a completion item of type EC_COMP_UNKNOWN, just to indicate
 * that everything before the last element of the string vector
 * has been parsed successfully, but we don't know how to
 * complete the last element.
 *
 * @param node
 *   The root node of the grammar tree.
 * @param comp
 *   The current list of completion items, to be filled by the
 *   node.type->complete() method.
 * @param strvec
 *   The string vector to be completed.
 * @return
 *   0 on success, or a negative value on error (errno is set).
 */
typedef int (*ec_complete_t)(const struct ec_node *node,
				struct ec_comp *comp,
				const struct ec_strvec *strvec);

/**
 *
 */
typedef const char * (*ec_node_desc_t)(const struct ec_node *);
typedef int (*ec_node_init_priv_t)(struct ec_node *);
typedef void (*ec_node_free_priv_t)(struct ec_node *);
typedef size_t (*ec_node_get_children_count_t)(const struct ec_node *);
typedef int (*ec_node_get_child_t)(const struct ec_node *,
	size_t i, struct ec_node **child, unsigned int *refs);

/**
 * A structure describing a node type.
 */
struct ec_node_type {
	TAILQ_ENTRY(ec_node_type) next;  /**< Next in list. */
	const char *name;                /**< Node type name. */
	/** Configuration schema array, must be terminated by a sentinel
	 *  (.type = EC_CONFIG_TYPE_NONE). */
	const struct ec_config_schema *schema;
	ec_node_set_config_t set_config; /* validate/ack a config change */
	ec_parse_t parse;
	ec_complete_t complete;
	ec_node_desc_t desc;
	size_t size;
	ec_node_init_priv_t init_priv;
	ec_node_free_priv_t free_priv;
	ec_node_get_children_count_t get_children_count;
	ec_node_get_child_t get_child;
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
const struct ec_node_type *ec_node_type_lookup(const char *name);

/**
 * Dump registered log types
 */
void ec_node_type_dump(FILE *out);

/**
 * Get the config schema of a node type.
 */
const struct ec_config_schema *
ec_node_type_schema(const struct ec_node_type *type);

/**
 * Get the name of a node type.
 */
const char *
ec_node_type_name(const struct ec_node_type *type);

enum ec_node_free_state {
	EC_NODE_FREE_STATE_NONE,
	EC_NODE_FREE_STATE_TRAVERSED,
	EC_NODE_FREE_STATE_FREEABLE,
	EC_NODE_FREE_STATE_NOT_FREEABLE,
	EC_NODE_FREE_STATE_FREEING,
};

/* create a new node when the type is known, typically called from the node
 * code */
struct ec_node *ec_node_from_type(const struct ec_node_type *type, const char *id);

/* create a new node */
struct ec_node *ec_node(const char *typename, const char *id);

struct ec_node *ec_node_clone(struct ec_node *node);
void ec_node_free(struct ec_node *node);

/* set configuration of a node
 * after a call to this function, the config is
 * owned by the node and must not be used by the caller
 * on error, the config is freed. */
int ec_node_set_config(struct ec_node *node, struct ec_config *config);

/* get the current node configuration. Return NULL if no configuration. */
const struct ec_config *ec_node_get_config(struct ec_node *node);

size_t ec_node_get_children_count(const struct ec_node *node);
int
ec_node_get_child(const struct ec_node *node, size_t i,
	struct ec_node **child, unsigned int *refs);

/* XXX add more accessors */
const struct ec_node_type *ec_node_type(const struct ec_node *node);
struct ec_dict *ec_node_attrs(const struct ec_node *node);
const char *ec_node_id(const struct ec_node *node);
const char *ec_node_desc(const struct ec_node *node);

void ec_node_dump(FILE *out, const struct ec_node *node);
struct ec_node *ec_node_find(struct ec_node *node, const char *id);

/* check the type of a node */
int ec_node_check_type(const struct ec_node *node,
		const struct ec_node_type *type);

const char *ec_node_get_type_name(const struct ec_node *node);

/**
 * Get the pointer to the node private area.
 *
 * @param node
 *   The grammar node.
 * @return
 *   The pointer to the node private area.
 */
void *ec_node_priv(const struct ec_node *node);

#endif

 /** @} */
