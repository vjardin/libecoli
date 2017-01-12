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

/// XXX rename in loop ?
//  XXX + provide the helper to free ?

/* a tk that just behaves like its child
 * useful to create cyclic graphs of tokens:
 *   creating a loop (with clones) result in something that is not
 *   freeable, due to reference counters
 * bypass node can solve the issue: before freeing the graph,
 * the loop can be cut, falling back to a valid tree that can
 * be freed.
 *
 * Example:
 *   seq = seq()
 *   i = int()
 *   seq_add(seq, i)
 *   seq_add(seq, clone(seq))
 *     FAIL, cannot be freed
 *
 *   seq = seq()
 *   bypass = bypass(clone(seq))
 *   i = int()
 *   seq_add(seq, i)
 *   seq_add(seq, bypass)
 *
 *   TO FREE:
 *     seq2 = bypass_del(bypass) // breaks the loop (seq2 == seq)
 *     free(bypass)
 *     free(seq2)
 *     free(seq)
 */

struct ec_tk *ec_tk_bypass(const char *id, struct ec_tk *child);

struct ec_tk *ec_tk_bypass_new(const char *id);

/* child is consumed */
/* all token given in the list will be freed when freeing this one */
int ec_tk_bypass_set(struct ec_tk *tk, struct ec_tk *child);

struct ec_tk *ec_tk_bypass_pop(struct ec_tk *gen_tk);

#endif
