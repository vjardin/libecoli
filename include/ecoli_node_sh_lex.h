/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

/**
 * @defgroup nodes Nodes
 * @{
 */

#ifndef ECOLI_NODE_SHLEX_
#define ECOLI_NODE_SHLEX_

#include <ecoli_node.h>

struct ec_node *ec_node_sh_lex(const char *id, struct ec_node *child);

#endif

/** @} */
