/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018, Olivier MATZ <zer0@droids-corp.org>
 */

/**
 * @defgroup ecoli_yaml YAML import/export
 * @{
 *
 * Interface to import/export ecoli data structures in YAML.
 */

#pragma once

struct ec_node;

/**
 * Parse a YAML file and build an ec_node tree from it.
 *
 * @param filename
 *   The path to the file to be parsed.
 * @return
 *   The ec_node tree on success, or NULL on error (errno is set).
 *   The returned node must be freed by the caller with ec_node_free().
 */
struct ec_node *ec_yaml_import(const char *filename);

/**
 * Export an ec_node tree to a YAML file.
 *
 * Write the YAML representation of the node tree rooted at 'root' into the
 * file located at 'filename'. The file is created (or truncated) and written
 * using the libyaml emitter. The function does not take ownership of or
 * modify the provided 'root' node.
 * The produced YAML mirrors the format accepted by ec_yaml_import().
 *
 * @param filename
 *   Path to the file to write. Must be a valid, writable path (not NULL).
 * @param root
 *  Pointer to the root ec_node to export (must not be NULL).
 * @return
 *   0 on success,
 *  -1 on failure. On failure errno is set to indicate the error:
 *     EINVAL: invalid argument or internal data inconsistency
 *     ENOMEM: memory allocation failure
 *     EIO: I/O or libyaml emit error
 *     Other errno values may be propagated from fopen() or underlying calls.
 */
int ec_yaml_export(const char *filename, const struct ec_node *root);

/** @} */
