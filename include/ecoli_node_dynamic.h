/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017, Olivier MATZ <zer0@droids-corp.org>
 */

/**
 * @defgroup nodes Nodes
 * @{
 */

#ifndef ECOLI_NODE_DYNAMIC_
#define ECOLI_NODE_DYNAMIC_

struct ec_node;
struct ec_pnode;

/* callback invoked by parse() or complete() to build the dynamic node
 * the behavior of the node can depend on what is already parsed */
typedef struct ec_node *(*ec_node_dynamic_build_t)(
	struct ec_pnode *state, void *opaque);

struct ec_node *ec_node_dynamic(const char *id, ec_node_dynamic_build_t build,
				void *opaque);

#endif

/** @} */
