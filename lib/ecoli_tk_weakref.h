/*
 * Copyright (c) 2016, Olivier MATZ <zer0@droids-corp.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University of California, Berkeley nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ECOLI_TK_BYPASS_
#define ECOLI_TK_BYPASS_

#include <ecoli_tk.h>

/* a tk that just behaves like its child and that does not free
 * its child when freed.
 *
 * useful to create cyclic graphs of tokens:
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
struct ec_tk *ec_tk_weakref(const char *id, struct ec_tk *child);

struct ec_tk *ec_tk_weakref_empty(const char *id);

/* on error, child is *not* freed */
int ec_tk_weakref_set(struct ec_tk *tk, struct ec_tk *child);

#endif
