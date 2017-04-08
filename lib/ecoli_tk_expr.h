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

#ifndef ECOLI_TK_EXPR_
#define ECOLI_TK_EXPR_

#include <ecoli_tk.h>

/* XXX remove the _new for all other tokens */

/* XXX explain the free policy in case of error or success */
struct ec_tk_expr_eval_result {
	int code; /* 0 on success, negative on error */
	void *parsed;
};

typedef struct ec_tk_expr_eval_result (*ec_tk_expr_eval_var_t)(
	void *userctx, const struct ec_parsed_tk *var);

typedef struct ec_tk_expr_eval_result (*ec_tk_expr_eval_pre_op_t)(
	void *userctx, struct ec_tk_expr_eval_result operand,
	const struct ec_parsed_tk *operator);

typedef struct ec_tk_expr_eval_result (*ec_tk_expr_eval_post_op_t)(
	void *userctx, struct ec_tk_expr_eval_result operand,
	const struct ec_parsed_tk *operator);

typedef struct ec_tk_expr_eval_result (*ec_tk_expr_eval_bin_op_t)(
	void *userctx, struct ec_tk_expr_eval_result operand1,
	const struct ec_parsed_tk *operator,
	struct ec_tk_expr_eval_result operand2);

typedef struct ec_tk_expr_eval_result (*ec_tk_expr_eval_parenthesis_t)(
	void *userctx, const struct ec_parsed_tk *operator_str,
	const struct ec_parsed_tk *operand_str,
	void *operand);


struct ec_tk *ec_tk_expr(const char *id);
int ec_tk_expr_set_val_tk(struct ec_tk *gen_tk, struct ec_tk *val_tk);
int ec_tk_expr_add_bin_op(struct ec_tk *gen_tk, struct ec_tk *op);
int ec_tk_expr_add_pre_op(struct ec_tk *gen_tk, struct ec_tk *op);
int ec_tk_expr_add_post_op(struct ec_tk *gen_tk, struct ec_tk *op);
int ec_tk_expr_add_parenthesis(struct ec_tk *gen_tk,
	struct ec_tk *open, struct ec_tk *close);

struct ec_tk_expr_eval_ops {
	ec_tk_expr_eval_var_t eval_var;
	ec_tk_expr_eval_pre_op_t eval_pre_op;
	ec_tk_expr_eval_post_op_t eval_post_op;
	ec_tk_expr_eval_bin_op_t eval_bin_op;
	ec_tk_expr_eval_parenthesis_t eval_parenthesis;
};

struct ec_tk_expr_eval_result ec_tk_expr_eval(const struct ec_tk *tk,
	struct ec_parsed_tk *parsed, const struct ec_tk_expr_eval_ops *ops,
	void *userctx);

#endif
