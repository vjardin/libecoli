/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#ifndef ECOLI_NODE_CMD_
#define ECOLI_NODE_CMD_

#include <ecoli_node.h>

#define EC_NODE_CMD(args...) __ec_node_cmd(args, EC_NODE_ENDLIST)

struct ec_node *__ec_node_cmd(const char *id, const char *cmd_str, ...);

struct ec_node *ec_node_cmd(const char *id, const char *cmd_str);

/* child is consumed */
int ec_node_cmd_add_child(struct ec_node *node, struct ec_node *child);

#endif
