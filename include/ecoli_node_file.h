/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

/**
 * @addtogroup ecoli_nodes
 * @{
 */

#ifndef ECOLI_NODE_FILE_
#define ECOLI_NODE_FILE_

#include <ecoli_node.h>

struct ec_node *ec_node_file(const char *id, const char *file);

/* file is duplicated */
int ec_node_file_set_str(struct ec_node *node, const char *file);

#endif

/** @} */
