/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

/**
 * @addtogroup ecoli_nodes
 * @{
 */

#ifndef ECOLI_NODE_EMPTY_
#define ECOLI_NODE_EMPTY_

/**
 * This node always matches an empty string vector
 */
struct ec_node *ec_node_empty(const char *id);

#endif

/** @} */
