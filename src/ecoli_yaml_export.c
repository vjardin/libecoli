/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025, Vincent JARDIN <vjardin@free.fr>
 * Export an ec_node to a YAML file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#include <yaml.h>

#include <ecoli_malloc.h>
#include <ecoli_dict.h>
#include <ecoli_node.h>
#include <ecoli_config.h>
#include <ecoli_log.h>
#include <ecoli_yaml.h>

EC_LOG_TYPE_REGISTER(yaml_export);

/* Helper that emits an event and logs on failure */
static int
emit_event(yaml_emitter_t *emitter, yaml_event_t *event, const char *msg)
{
	if (!yaml_emitter_emit(emitter, event)) {
		int save_errno = errno;
		EC_LOG(EC_LOG_ERR, "yaml_emitter_emit failed: %s\n", msg ? msg : "");
		errno = (save_errno == 0) ? EIO : save_errno;
		return -1;
	}
	return 0;
}

static int
emit_scalar(yaml_emitter_t *emitter, const char *str)
{
	yaml_event_t event;
	if (!yaml_scalar_event_initialize(&event,
					 NULL, /* anchor */
					 NULL, /* tag */
					 (yaml_char_t *)str,
					 (int)strlen(str),
					 1, /* plain implicit */
					 1, /* quoted implicit */
					 YAML_PLAIN_SCALAR_STYLE)) {
		EC_LOG(EC_LOG_ERR, "yaml_scalar_event_initialize failed\n");
		errno = ENOMEM;
		return -1;
	}
	return emit_event(emitter, &event, "scalar");
}

static int
emit_mapping_start(yaml_emitter_t *emitter)
{
	yaml_event_t event;
	if (!yaml_mapping_start_event_initialize(&event,
						NULL, NULL,
						1, /* implicit */
						YAML_BLOCK_MAPPING_STYLE)) {
		EC_LOG(EC_LOG_ERR, "yaml_mapping_start_event_initialize failed\n");
		errno = ENOMEM;
		return -1;
	}
	return emit_event(emitter, &event, "mapping start");
}

static int
emit_mapping_end(yaml_emitter_t *emitter)
{
	yaml_event_t event;
	if (!yaml_mapping_end_event_initialize(&event)) {
		EC_LOG(EC_LOG_ERR, "yaml_mapping_end_event_initialize failed\n");
		errno = ENOMEM;
		return -1;
	}
	return emit_event(emitter, &event, "mapping end");
}

static int
emit_sequence_start(yaml_emitter_t *emitter)
{
	yaml_event_t event;
	if (!yaml_sequence_start_event_initialize(&event,
						  NULL, NULL,
						  1, YAML_BLOCK_SEQUENCE_STYLE)) {
		EC_LOG(EC_LOG_ERR, "yaml_sequence_start_event_initialize failed\n");
		errno = ENOMEM;
		return -1;
	}
	return emit_event(emitter, &event, "sequence start");
}

static int
emit_sequence_end(yaml_emitter_t *emitter)
{
	yaml_event_t event;
	if (!yaml_sequence_end_event_initialize(&event)) {
		EC_LOG(EC_LOG_ERR, "yaml_sequence_end_event_initialize failed\n");
		errno = ENOMEM;
		return -1;
	}
	return emit_event(emitter, &event, "sequence end");
}

/* Forward declaration */
static int export_ec_config(yaml_emitter_t *emitter, const struct ec_config *cfg);
static int export_ec_node(yaml_emitter_t *emitter, const struct ec_node *node);

/* Emit attrs mapping (skip "help" key - emitted separately) */
static int
export_attributes(yaml_emitter_t *emitter, const struct ec_node *node)
{
	struct ec_dict *attrs = ec_node_attrs((struct ec_node *)node);
	if (attrs == NULL)
		return 0;

	/* iterate dict and count keys != "help" first to avoid empty "attrs" */
	struct ec_dict_elt_ref *iter;
	bool has_other = false;
	for (iter = ec_dict_iter(attrs); iter != NULL; iter = ec_dict_iter_next(iter)) {
		const char *k = ec_dict_iter_get_key(iter);
		if (k == NULL)
			continue;
		if (strcmp(k, "help") == 0)
			continue;
		has_other = true;
		break;
	}
	if (!has_other)
		return 0;

	if (emit_scalar(emitter, "attrs") < 0)
		return -1;
	if (emit_mapping_start(emitter) < 0)
		return -1;

	for (iter = ec_dict_iter(attrs); iter != NULL; iter = ec_dict_iter_next(iter)) {
		const char *k = ec_dict_iter_get_key(iter);
		void *v = ec_dict_iter_get_val(iter);
		const char *vs = v ? (const char *)v : "";
		if (k == NULL)
			continue;
		if (strcmp(k, "help") == 0)
			continue;

		if (emit_scalar(emitter, k) < 0)
			return -1;
		if (emit_scalar(emitter, vs) < 0)
			return -1;
	}

	if (emit_mapping_end(emitter) < 0)
		return -1;

	return 0;
}

/* Emit a node as a YAML mapping */
static int
export_ec_node(yaml_emitter_t *emitter, const struct ec_node *node)
{
	/* mapping start for the node */
	if (emit_mapping_start(emitter) < 0)
		return -1;

	/* type */
	if (emit_scalar(emitter, "type") < 0)
		return -1;
	if (emit_scalar(emitter, ec_node_type_name(ec_node_type((struct ec_node *)node))) < 0)
		return -1;

	/* id if set and not EC_NO_ID */
	{
		const char *id = ec_node_id((struct ec_node *)node);
		if (id != NULL && strcmp(id, EC_NO_ID) != 0) {
			if (emit_scalar(emitter, "id") < 0)
				return -1;
			if (emit_scalar(emitter, id) < 0)
				return -1;
		}
	}

	/* configuration: top-level node config is expected to be a dict */
	{
		const struct ec_config *cfg = ec_node_get_config((struct ec_node *)node);
		if (cfg != NULL) {
			if (ec_config_get_type(cfg) != EC_CONFIG_TYPE_DICT) {
				EC_LOG(EC_LOG_ERR, "node config is not a dict\n");
				errno = EINVAL;
				return -1;
			}

			/* cfg->dict is accessible here because struct ec_config is in the header */
			struct ec_dict *cdict = cfg->dict;
			if (cdict != NULL) {
				struct ec_dict_elt_ref *dit;
				for (dit = ec_dict_iter(cdict); dit != NULL; dit = ec_dict_iter_next(dit)) {
					const char *k = ec_dict_iter_get_key(dit);
					void *v = ec_dict_iter_get_val(dit);
					struct ec_config *sub = (struct ec_config *)v;
					if (k == NULL)
						continue;

					/* skip reserved keys if any (importer ignores reserved keys) */
					if (ec_config_key_is_reserved(k))
						continue;

					if (emit_scalar(emitter, k) < 0)
						return -1;
					if (export_ec_config(emitter, sub) < 0)
						return -1;
				}
			}
		}
	}

	/* help attribute emitted as top-level "help" like importer expects */
	{
		struct ec_dict *attrs = ec_node_attrs((struct ec_node *)node);
		if (attrs != NULL) {
			void *hv = ec_dict_get(attrs, "help");
			/* ec_dict_get returns NULL on error or if value is NULL; check has_key to distinguish */
			if (ec_dict_has_key(attrs, "help")) {
				const char *help = hv ? (const char *)hv : "";
				if (emit_scalar(emitter, "help") < 0)
					return -1;
				if (emit_scalar(emitter, help) < 0)
					return -1;
			}
		}
	}

	/* attrs mapping with remaining attributes (except help) */
	if (export_attributes(emitter, node) < 0)
		return -1;

	/* mapping end for node */
	if (emit_mapping_end(emitter) < 0)
		return -1;

	return 0;
}

/* Export ec_config recursively */
static int
export_ec_config(yaml_emitter_t *emitter, const struct ec_config *cfg)
{
	enum ec_config_type type = ec_config_get_type(cfg);

	switch (type) {
	case EC_CONFIG_TYPE_BOOL: {
		const char *s = cfg->boolean ? "true" : "false";
		return emit_scalar(emitter, s);
	}
	case EC_CONFIG_TYPE_INT64: {
		char buf[64];
		snprintf(buf, sizeof(buf), "%" PRId64, cfg->i64);
		return emit_scalar(emitter, buf);
	}
	case EC_CONFIG_TYPE_UINT64: {
		char buf[64];
		snprintf(buf, sizeof(buf), "%" PRIu64, cfg->u64);
		return emit_scalar(emitter, buf);
	}
	case EC_CONFIG_TYPE_STRING: {
		const char *s = cfg->string ? cfg->string : "";
		return emit_scalar(emitter, s);
	}
	case EC_CONFIG_TYPE_NODE: {
		if (cfg->node == NULL) {
			EC_LOG(EC_LOG_ERR, "ec_config: node value is NULL\n");
			errno = EINVAL;
			return -1;
		}
		return export_ec_node(emitter, cfg->node);
	}
	case EC_CONFIG_TYPE_LIST: {
		/* iterate using public list helpers */
		struct ec_config *elt;

		if (emit_sequence_start(emitter) < 0)
			return -1;

		for (elt = ec_config_list_first((struct ec_config *)cfg);
		     elt != NULL;
		     elt = ec_config_list_next((struct ec_config *)cfg, elt)) {
			if (export_ec_config(emitter, elt) < 0) {
				/* elt ownership is not transferred here */
				return -1;
			}
		}

		if (emit_sequence_end(emitter) < 0)
			return -1;
		return 0;
	}
	case EC_CONFIG_TYPE_DICT: {
		/* cfg->dict is a struct ec_dict*; iterate using ec_dict APIs */
		struct ec_dict *cdict = cfg->dict;
		if (emit_mapping_start(emitter) < 0)
			return -1;

		if (cdict != NULL) {
			struct ec_dict_elt_ref *dit;
			for (dit = ec_dict_iter(cdict); dit != NULL; dit = ec_dict_iter_next(dit)) {
				const char *k = ec_dict_iter_get_key(dit);
				void *v = ec_dict_iter_get_val(dit);
				struct ec_config *sub = (struct ec_config *)v;
				if (k == NULL)
					continue;

				if (emit_scalar(emitter, k) < 0)
					return -1;
				if (export_ec_config(emitter, sub) < 0)
					return -1;
			}
		}

		if (emit_mapping_end(emitter) < 0)
			return -1;
		return 0;
	}
	default:
		EC_LOG(EC_LOG_ERR, "export_ec_config: unsupported type %d\n", type);
		errno = EINVAL;
		return -1;
	}
}

/* Public function: write YAML to file. Return 0 on success, -1 on error. */
int
ec_yaml_export(const char *filename, const struct ec_node *root)
{
	FILE *file = NULL;
	yaml_emitter_t emitter;
	yaml_event_t event;
	int rc = -1;

	if (filename == NULL || root == NULL) {
		EC_LOG(EC_LOG_ERR, "ec_yaml_export: invalid arguments\n");
		errno = EINVAL;
		return -1;
	}

	file = fopen(filename, "wb");
	if (file == NULL) {
		EC_LOG(EC_LOG_ERR, "ec_yaml_export: failed to open %s: %s\n",
		       filename, strerror(errno));
		/* errno already set by fopen */
		return -1;
	}

	if (!yaml_emitter_initialize(&emitter)) {
		EC_LOG(EC_LOG_ERR, "ec_yaml_export: yaml_emitter_initialize failed\n");
		errno = ENOMEM;
		goto out_close;
	}

	yaml_emitter_set_output_file(&emitter, file);

	/* stream start */
	if (!yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING)) {
		EC_LOG(EC_LOG_ERR, "ec_yaml_export: stream_start init failed\n");
		errno = ENOMEM;
		goto out_emitter;
	}
	if (emit_event(&emitter, &event, "stream start") < 0)
		goto out_emitter;

	/* document start */
	if (!yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0)) {
		EC_LOG(EC_LOG_ERR, "ec_yaml_export: document_start init failed\n");
		errno = ENOMEM;
		goto out_emitter;
	}
	if (emit_event(&emitter, &event, "document start") < 0)
		goto out_emitter;

	/* emit the root node */
	if (export_ec_node(&emitter, root) < 0)
		goto out_emitter;

	/* document end */
	if (!yaml_document_end_event_initialize(&event, 0)) {
		EC_LOG(EC_LOG_ERR, "ec_yaml_export: document_end init failed\n");
		errno = ENOMEM;
		goto out_emitter;
	}
	if (emit_event(&emitter, &event, "document end") < 0)
		goto out_emitter;

	/* stream end */
	if (!yaml_stream_end_event_initialize(&event)) {
		EC_LOG(EC_LOG_ERR, "ec_yaml_export: stream_end init failed\n");
		errno = ENOMEM;
		goto out_emitter;
	}
	if (emit_event(&emitter, &event, "stream end") < 0)
		goto out_emitter;

	rc = 0;

out_emitter:
	yaml_emitter_delete(&emitter);
out_close:
	fclose(file);
	return rc;
}
