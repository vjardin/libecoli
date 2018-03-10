/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#ifndef ECOLI_NODE_WEAKREF_
#define ECOLI_NODE_WEAKREF_

#include <ecoli_node.h>

/* A node that just behaves like its child and that does not free
 * its child when freed. **The child has to be freed manually**.
 *
 * useful to create cyclic graphs of nodes:
 *   creating a loop (with clones) result in something that is not
 *   freeable, due to reference counters
 *
 * Example:
 *  val = int(0, 10)
 *  op = str("!")
 *  expr = or()
 *  seq = seq(clone(op), clone(expr))
 *  expr.add(clone(seq))
 *  expr.add(clone(val))
 *  free(val)
 *  free(op)
 *  free(seq)
 *
 * FAIL: expr cannot be freed due to cyclic refs
 * The references are like this:
 *
 *                   val
 *                    ^
 *                    |
 *        $user ---> expr ---> seq ---> op
 *                        <---
 *
 * It is solved with:
 *  val = int(0, 10)
 *  op = str("!")
 *  expr = or()
 *  weak = weak(expr)
 *  seq = seq(clone(op), clone(weak))
 *  expr.add(clone(seq))
 *  expr.add(clone(val))
 *  free(val)
 *  free(op)
 *  free(weak)
 *  free(seq)
 *
 *
 *                   val
 *                    ^
 *                    |
 *        $user ---> expr ---------------> seq ---> op
 *                        <- - - weak <---
 *
 * The node expr can be freed.
 */

/* on error, child is *not* freed */
struct ec_node *ec_node_weakref(const char *id, struct ec_node *child);

/* on error, child is *not* freed */
int ec_node_weakref_set(struct ec_node *node, struct ec_node *child);

#endif
