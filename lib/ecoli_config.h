/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018, Olivier MATZ <zer0@droids-corp.org>
 */

#ifndef ECOLI_CONFIG_
#define ECOLI_CONFIG_

#include <stdbool.h>
#include <stdint.h>

struct ec_config;
struct ec_keyval;

/**
 * The type identifier for a config value.
 */
enum ec_config_type {
	EC_CONFIG_TYPE_NONE = 0,
	EC_CONFIG_TYPE_BOOL,
	EC_CONFIG_TYPE_INT64,
	EC_CONFIG_TYPE_UINT64,
	EC_CONFIG_TYPE_STRING,
	EC_CONFIG_TYPE_NODE,
	EC_CONFIG_TYPE_LIST,
	EC_CONFIG_TYPE_DICT,
};

/**
 * Structure describing the format of a configuration value.
 *
 * This structure is used in a const array which is referenced by a
 * struct ec_config. Each entry of the array represents a key/value
 * storage of the configuration dictionary.
 */
struct ec_config_schema {
	const char *key;          /**< The key string (NULL for list elts). */
	const char *desc;         /**< A description of the value. */
	enum ec_config_type type; /**< Type of the value */

	/** If type is dict or list, the schema of the dict or list
	 * elements. Else must be NULL. */
	const struct ec_config_schema *subschema;

	/** The subschema array len in case of dict (> 0) or list (set
	 * to 1). Else must be 0. */
	size_t subschema_len;

};

TAILQ_HEAD(ec_config_list, ec_config);

/**
 * Structure storing data.
 */
struct ec_config {
	/** type of value stored in the union */
	enum ec_config_type type;

	union {
		bool boolean;
		int64_t i64;
		uint64_t u64;
		char *string;
		struct ec_node *node;
		struct ec_keyval *dict;
		struct ec_config_list list;
	};

	/**
	 * Valid if type is list.
	 */
	TAILQ_ENTRY(ec_config) next;

	/** Associated schema. Can be set if type is dict. */
	const struct ec_config_schema *schema;
	size_t schema_len;           /**< Schema length. */
};

/* schema */

int ec_config_schema_validate(const struct ec_config_schema *schema,
			size_t schema_len);

void ec_config_schema_dump(FILE *out, const struct ec_config_schema *schema,
			size_t schema_len);


/* config */

struct ec_config *ec_config_bool(bool boolean);
struct ec_config *ec_config_i64(int64_t i64);
struct ec_config *ec_config_u64(uint64_t u64);
/* duplicate string */
struct ec_config *ec_config_string(const char *string);
/* "consume" the node */
struct ec_config *ec_config_node(struct ec_node *node);
/* "consume" the dict */
struct ec_config *ec_config_dict(void);
struct ec_config *ec_config_list(void);

int ec_config_add(struct ec_config *list, struct ec_config *value);

int ec_config_set_schema(struct ec_config *dict,
			const struct ec_config_schema *schema,
			size_t schema_len);

void ec_config_free(struct ec_config *config);

int ec_config_validate(const struct ec_config *dict);

/* value is consumed */
int ec_config_set(struct ec_config *dict, const char *key,
		struct ec_config *value);

/**
 * Compare two configurations.
 */
int ec_config_cmp(const struct ec_config *config1,
		const struct ec_config *config2);

/**
 * Get configuration value.
 */
const struct ec_config *ec_config_get(const struct ec_config *config,
				const char *key);

/* */

const struct ec_config_schema *
ec_config_schema_lookup(const struct ec_config_schema *schema,
			size_t schema_len, const char *key,
			enum ec_config_type type);

void ec_config_free(struct ec_config *value);

int ec_config_cmp(const struct ec_config *value1,
		const struct ec_config *value2);

char *ec_config_str(const struct ec_config *value);

void ec_config_dump(FILE *out, const struct ec_config *config);

#endif
