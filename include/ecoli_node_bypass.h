/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019, Olivier MATZ <zer0@droids-corp.org>
 */

/**
 * A node that does nothing else than calling the child node.
 * It can be helpful to build loops in a node graph.
 */

#ifndef ECOLI_NODE_BYPASS_
#define ECOLI_NODE_BYPASS_

#include <ecoli_node.h>

struct ec_node *ec_node_bypass(const char *id, struct ec_node *node);
int ec_node_bypass_set_child(struct ec_node *gen_node, struct ec_node *child);

#endif
