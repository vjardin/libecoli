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
 *  expr = or()
 *  val = int(0, 10)
 *  op = str("!")
 *  seq = seq(op, clone(expr))
 *  expr.add(seq)
 *  expr.add(val)
 *  free(expr) // just decrease ref
 *
 * FAIL: expr cannot be freed due to cyclic refs
 * The references are like this:
 *
 *                   val
 *                    ^
 *                    |
 *     $user ~ ~ ~ > expr ---> seq ---> op
 *                        <---
 *
 * It is solved with:
 *  expr = or()
 *  val = int(0, 10)
 *  op = str("!")
 *  weak = weak(expr)
 *  seq = seq(op, weak)
 *  expr.add(op)
 *  expr.add(val)
 *
 *
 *                   val
 *                    ^
 *                    |
 *     $user ~ ~ ~ > expr ---------------> seq ---> op
 *                        <- - - weak <---
 *
 * The node expr can be freed.
 */

/* on error, child is *not* freed */
struct ec_node *ec_node_weakref(const char *id, struct ec_node *child);

/* on error, child is *not* freed */
int ec_node_weakref_set(struct ec_node *node, struct ec_node *child);

#endif
