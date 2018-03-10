/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#ifndef ECOLI_NODE_RE_LEX_
#define ECOLI_NODE_RE_LEX_

#include <ecoli_node.h>

struct ec_node *ec_node_re_lex(const char *id, struct ec_node *child);

int ec_node_re_lex_add(struct ec_node *gen_node, const char *pattern, int keep);

#endif
