/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

/**
 * @defgroup nodes Nodes
 * @{
 */

#ifndef ECOLI_NODE_OR_
#define ECOLI_NODE_OR_

#include <ecoli_node.h>
#include <ecoli_utils.h>

#define EC_NODE_OR(args...) __ec_node_or(args, EC_VA_END)

/* list must be terminated with EC_VA_END */
/* all nodes given in the list will be freed when freeing this one */
/* avoid using this function directly, prefer the macro EC_NODE_OR() or
 * ec_node_or() + ec_node_or_add() */
struct ec_node *__ec_node_or(const char *id, ...);

struct ec_node *ec_node_or(const char *id);

/* child is consumed */
int ec_node_or_add(struct ec_node *node, struct ec_node *child);

#endif

/** @} */
