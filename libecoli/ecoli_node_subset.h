/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#ifndef ECOLI_NODE_SUBSET_
#define ECOLI_NODE_SUBSET_

#include <ecoli_node.h>

#define EC_NODE_SUBSET(args...) __ec_node_subset(args, EC_NODE_ENDLIST)

/* list must be terminated with EC_NODE_ENDLIST */
/* all nodes given in the list will be freed when freeing this one */
/* avoid using this function directly, prefer the macro EC_NODE_SUBSET() or
 * ec_node_subset() + ec_node_subset_add() */
struct ec_node *__ec_node_subset(const char *id, ...);

struct ec_node *ec_node_subset(const char *id);

/* child is consumed */
int ec_node_subset_add(struct ec_node *node, struct ec_node *child);

#endif
