#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>

#include <yaml.h>
#include <ecoli_node.h>
#include <ecoli_config.h>

/* associate a yaml node to a ecoli node */
struct pair {
	const yaml_node_t *ynode;
	struct ec_node *enode;
};

/* store the ecoli node tree and the associations yaml_node <-> ec_node */
struct enode_tree {
	struct ec_node *root;
	struct pair *table;
	size_t table_len;
};

static struct ec_node *
parse_ec_node(struct enode_tree *tree,
	const yaml_document_t *document, const yaml_node_t *ynode);

static struct ec_config *
parse_ec_config_list(struct enode_tree *tree,
		const struct ec_config_schema *schema,
		const yaml_document_t *document, const yaml_node_t *ynode);

static struct ec_config *
parse_ec_config_dict(struct enode_tree *tree,
		const struct ec_config_schema *schema,
		const yaml_document_t *document, const yaml_node_t *ynode);

/* XXX to utils.c ? */
static int
parse_llint(const char *str, int64_t *val)
{
	char *endptr;
	int save_errno = errno;

	errno = 0;
	*val = strtoll(str, &endptr, 0);

	if ((errno == ERANGE && (*val == LLONG_MAX || *val == LLONG_MIN)) ||
			(errno != 0 && *val == 0))
		return -1;

	if (*endptr != 0) {
		errno = EINVAL;
		return -1;
	}

	errno = save_errno;
	return 0;
}

static int
parse_ullint(const char *str, uint64_t *val)
{
	char *endptr;
	int save_errno = errno;

	/* since a negative input is silently converted to a positive
	 * one by strtoull(), first check that it is positive */
	if (strchr(str, '-'))
		return -1;

	errno = 0;
	*val = strtoull(str, &endptr, 0);

	if ((errno == ERANGE && *val == ULLONG_MAX) ||
			(errno != 0 && *val == 0))
		return -1;

	if (*endptr != 0)
		return -1;

	errno = save_errno;
	return 0;
}

static int
parse_bool(const char *str, bool *val)
{
	if (!strcasecmp(str, "true")) {
		*val = true;
		return 0;
	} else if (!strcasecmp(str, "false")) {
		*val = false;
		return 0;
	}
	errno = EINVAL;
	return -1;
}

static int
add_in_table(struct enode_tree *tree, const yaml_node_t *ynode,
	struct ec_node *enode)
{
	struct pair *table = NULL;

	table = realloc(tree->table, (tree->table_len + 1) * sizeof(*table));
	if (table == NULL)
		return -1;

	ec_node_clone(enode);
	table[tree->table_len].ynode = ynode;
	table[tree->table_len].enode = enode;
	tree->table = table;
	tree->table_len++;

	return 0;
}

static void
free_tree(struct enode_tree *tree)
{
	size_t i;

	if (tree->root != NULL)
		ec_node_free(tree->root);
	for (i = 0; i < tree->table_len; i++)
		ec_node_free(tree->table[i].enode);
	free(tree->table);
}

static struct ec_config *
parse_ec_config(struct enode_tree *tree,
		const struct ec_config_schema *schema_elt,
		const yaml_document_t *document, const yaml_node_t *ynode)
{
	const struct ec_config_schema *subschema;
	struct ec_config *config = NULL;
	struct ec_node *enode = NULL;
	enum ec_config_type type;
	const char *value_str;
	uint64_t u64;
	int64_t i64;
	bool boolean;

	type = ec_config_schema_type(schema_elt);

	switch (type) {
	case EC_CONFIG_TYPE_BOOL:
		if (ynode->type != YAML_SCALAR_NODE) {
			fprintf(stderr, "Boolean should be scalar\n");
			goto fail;
		}
		value_str = (const char *)ynode->data.scalar.value;
		if (parse_bool(value_str, &boolean)  < 0) {
			fprintf(stderr, "Failed to parse boolean\n");
			goto fail;
		}
		config = ec_config_bool(boolean);
		if (config == NULL) {
			fprintf(stderr, "Failed to create config\n");
			goto fail;
		}
		break;
	case EC_CONFIG_TYPE_INT64:
		if (ynode->type != YAML_SCALAR_NODE) {
			fprintf(stderr, "Int64 should be scalar\n");
			goto fail;
		}
		value_str = (const char *)ynode->data.scalar.value;
		if (parse_llint(value_str, &i64)  < 0) {
			fprintf(stderr, "Failed to parse i64\n");
			goto fail;
		}
		config = ec_config_i64(i64);
		if (config == NULL) {
			fprintf(stderr, "Failed to create config\n");
			goto fail;
		}
		break;
	case EC_CONFIG_TYPE_UINT64:
		if (ynode->type != YAML_SCALAR_NODE) {
			fprintf(stderr, "Uint64 should be scalar\n");
			goto fail;
		}
		value_str = (const char *)ynode->data.scalar.value;
		if (parse_ullint(value_str, &u64)  < 0) {
			fprintf(stderr, "Failed to parse u64\n");
			goto fail;
		}
		config = ec_config_u64(u64);
		if (config == NULL) {
			fprintf(stderr, "Failed to create config\n");
			goto fail;
		}
		break;
	case EC_CONFIG_TYPE_STRING:
		if (ynode->type != YAML_SCALAR_NODE) {
			fprintf(stderr, "String should be scalar\n");
			goto fail;
		}
		value_str = (const char *)ynode->data.scalar.value;
		config = ec_config_string(value_str);
		if (config == NULL) {
			fprintf(stderr, "Failed to create config\n");
			goto fail;
		}
		break;
	case EC_CONFIG_TYPE_NODE:
		enode = parse_ec_node(tree, document, ynode);
		if (enode == NULL)
			goto fail;
		config = ec_config_node(enode);
		if (config == NULL) {
			fprintf(stderr, "Failed to create config\n");
			goto fail;
		}
		break;
	case EC_CONFIG_TYPE_LIST:
		subschema = ec_config_schema_sub(schema_elt);
		if (subschema == NULL) {
			fprintf(stderr, "List has no subschema\n");
			goto fail;
		}
		config = parse_ec_config_list(tree, subschema, document, ynode);
		if (config == NULL)
			goto fail;
		break;
	case EC_CONFIG_TYPE_DICT:
		subschema = ec_config_schema_sub(schema_elt);
		if (subschema == NULL) {
			fprintf(stderr, "Dict has no subschema\n");
			goto fail;
		}
		config = parse_ec_config_dict(tree, subschema, document, ynode);
		if (config == NULL)
			goto fail;
		break;
	default:
		fprintf(stderr, "Invalid config type %d\n", type);
		goto fail;
	}

	return config;

fail:
	ec_node_free(enode);
	ec_config_free(config);
	return NULL;
}

static struct ec_config *
parse_ec_config_list(struct enode_tree *tree,
		const struct ec_config_schema *schema,
		const yaml_document_t *document, const yaml_node_t *ynode)
{
	struct ec_config *config = NULL, *subconfig = NULL;
	const yaml_node_item_t *item;
	const yaml_node_t *value;

	(void)tree;
	(void)schema;
	(void)document;

	if (ynode->type != YAML_SEQUENCE_NODE) {
		fprintf(stderr, "Ecoli list config should be a yaml sequence\n");
		goto fail;
	}

	config = ec_config_list();
	if (config == NULL) {
		fprintf(stderr, "Failed to allocate config\n");
		goto fail;
	}

	for (item = ynode->data.sequence.items.start;
	     item < ynode->data.sequence.items.top; item++) {
		value = document->nodes.start + (*item) - 1; // XXX -1 ?
		subconfig = parse_ec_config(tree, schema, document, value);
		if (subconfig == NULL)
			goto fail;
		if (ec_config_list_add(config, subconfig) < 0) {
			fprintf(stderr, "Failed to add list entry\n");
			goto fail;
		}
	}

	return config;

fail:
	ec_config_free(config);
	return NULL;
}

static struct ec_config *
parse_ec_config_dict(struct enode_tree *tree,
		const struct ec_config_schema *schema,
		const yaml_document_t *document, const yaml_node_t *ynode)
{
	const struct ec_config_schema *schema_elt;
	struct ec_config *config = NULL, *subconfig = NULL;
	const yaml_node_t *key, *value;
	const yaml_node_pair_t *pair;
	const char *key_str;

	if (ynode->type != YAML_MAPPING_NODE) {
		fprintf(stderr, "Ecoli config should be a yaml mapping node\n");
		goto fail;
	}

	config = ec_config_dict();
	if (config == NULL) {
		fprintf(stderr, "Failed to allocate config\n");
		goto fail;
	}

	for (pair = ynode->data.mapping.pairs.start;
	     pair < ynode->data.mapping.pairs.top; pair++) {
		key = document->nodes.start + pair->key - 1; // XXX -1 ?
		value = document->nodes.start + pair->value - 1;
		key_str = (const char *)key->data.scalar.value;

		if (ec_config_key_is_reserved(key_str))
			continue;
		schema_elt = ec_config_schema_lookup(schema, key_str);
		if (schema_elt == NULL) {
			fprintf(stderr, "No such config %s\n", key_str);
			goto fail;
		}
		subconfig = parse_ec_config(tree, schema_elt, document, value);
		if (subconfig == NULL)
			goto fail;
		if (ec_config_dict_set(config, key_str, subconfig) < 0) {
			fprintf(stderr, "Failed to set dict entry\n");
			goto fail;
		}
	}

	return config;

fail:
	ec_config_free(config);
	return NULL;
}

static struct ec_node *
parse_ec_node(struct enode_tree *tree,
	const yaml_document_t *document, const yaml_node_t *ynode)
{
	const struct ec_config_schema *schema;
	const struct ec_node_type *type = NULL;
	const char *id = NULL, *help = NULL;
	struct ec_config *config = NULL;
	const yaml_node_t *attrs = NULL;
	const yaml_node_t *key, *value;
	const yaml_node_pair_t *pair;
	const char *key_str, *value_str;
	struct ec_node *enode = NULL;

	if (ynode->type != YAML_MAPPING_NODE) {
		fprintf(stderr, "Ecoli node should be a yaml mapping node\n");
		goto fail;
	}

	for (pair = ynode->data.mapping.pairs.start;
	     pair < ynode->data.mapping.pairs.top; pair++) {
		key = document->nodes.start + pair->key - 1; // XXX -1 ?
		value = document->nodes.start + pair->value - 1;
		key_str = (const char *)key->data.scalar.value;
		value_str = (const char *)value->data.scalar.value;

		if (!strcmp(key_str, "type")) {
			if (type != NULL) {
				fprintf(stderr, "Duplicate type\n");
				goto fail;
			}
			if (value->type != YAML_SCALAR_NODE) {
				fprintf(stderr, "Type must be a string\n");
				goto fail;
			}
			type = ec_node_type_lookup(value_str);
			if (type == NULL) {
				fprintf(stderr, "Cannot find type %s\n",
					value_str);
				goto fail;
			}
		} else if (!strcmp(key_str, "attrs")) {
			if (attrs != NULL) {
				fprintf(stderr, "Duplicate attrs\n");
				goto fail;
			}
			if (value->type != YAML_MAPPING_NODE) {
				fprintf(stderr, "Attrs must be a maping\n");
				goto fail;
			}
			attrs = value;
		} else if (!strcmp(key_str, "id")) {
			if (id != NULL) {
				fprintf(stderr, "Duplicate id\n");
				goto fail;
			}
			if (value->type != YAML_SCALAR_NODE) {
				fprintf(stderr, "Id must be a scalar\n");
				goto fail;
			}
			id = value_str;
		} else if (!strcmp(key_str, "help")) {
			if (help != NULL) {
				fprintf(stderr, "Duplicate help\n");
				goto fail;
			}
			if (value->type != YAML_SCALAR_NODE) {
				fprintf(stderr, "Help must be a scalar\n");
				goto fail;
			}
			help = value_str;
		}
	}

	/* create the ecoli node */
	if (id == NULL)
		id = EC_NO_ID;
	enode = ec_node_from_type(type, id);
	if (enode == NULL) {
		fprintf(stderr, "Cannot create ecoli node\n");
		goto fail;
	}
	if (add_in_table(tree, ynode, enode) < 0) {
		fprintf(stderr, "Cannot add node in table\n");
		goto fail;
	}
	if (tree->root == NULL) {
		ec_node_clone(enode);
		tree->root = enode;
	}

	/* create its config */
	schema = ec_node_type_schema(type);
	if (schema == NULL) {
		fprintf(stderr, "No configuration schema for type %s\n",
			ec_node_type_name(type));
		goto fail;
	}

	config = parse_ec_config_dict(tree, schema, document, ynode);
	if (config == NULL)
		goto fail;

	if (ec_node_set_config(enode, config) < 0) {
		fprintf(stderr, "Failed to set config\n");
		goto fail;
	}

	/* add attributes (all as string) */
	//XXX

	return enode;

fail:
	ec_node_free(enode);
	ec_config_free(config);
	return NULL;
}

static int
parse_document(struct enode_tree *tree, const yaml_document_t *document)
{
	yaml_node_t *node;

	node = document->nodes.start;
	if (parse_ec_node(tree, document, node) == NULL)
		return -1;

	return 0;
}

static int parse_file(struct enode_tree *tree, const char *filename)
{
        FILE *file;
        yaml_parser_t parser;
        yaml_document_t document;

        file = fopen(filename, "rb");
	if (file == NULL) {
		fprintf(stderr, "Failed to open file %s\n", filename);
		goto fail_no_doc;
	}

        if (yaml_parser_initialize(&parser) == 0) {
		fprintf(stderr, "Failed to initialize yaml parser\n");
		goto fail_no_doc;
	}

        yaml_parser_set_input_file(&parser, file);

	if (yaml_parser_load(&parser, &document) == 0) {
		fprintf(stderr, "Failed to load yaml document\n");
		goto fail_no_doc;
	}

	if (yaml_document_get_root_node(&document) == NULL) {
		fprintf(stderr, "Incomplete document\n"); //XXX check err
		goto fail;
	}

	if (parse_document(tree, &document) < 0) {
		fprintf(stderr, "Failed to parse document\n");
		goto fail;
	}

	yaml_document_delete(&document);
        yaml_parser_delete(&parser);
        fclose(file);

	return 0;

fail:
	yaml_document_delete(&document);
fail_no_doc:
        yaml_parser_delete(&parser);
        if (file != NULL)
		fclose(file);

	return -1;
}

int
main(int argc, char *argv[])
{
	struct enode_tree tree;

	memset(&tree, 0, sizeof(tree));

	if (argc != 2) {
		fprintf(stderr, "Invalid args\n");
		goto fail;
	}
	if (parse_file(&tree, argv[1]) < 0) {
		fprintf(stderr, "Failed to parse file\n");
		goto fail;
	}
	printf("root=%p len=%zd\n", tree.root, tree.table_len);
	ec_node_dump(stdout, tree.root);
	free_tree(&tree);

	return 0;

fail:
	free_tree(&tree);
	return 1;
}
