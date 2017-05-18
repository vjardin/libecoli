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

/**
 * Callback function type for evaluating a variable
 *
 * @param result
 *   On success, this pointer must be set by the user to point
 *   to a user structure describing the evaluated result.
 * @param userctx
 *   A user-defined context passed to all callback functions, which
 *   can be used to maintain a state or store global information.
 * @param var
 *   The parsed token referencing the variable.
 * @return
 *   0 on success (*result must be set), or -errno on error (*result
 *   is undefined).
 */
typedef int (*ec_tk_expr_eval_var_t)(
	void **result, void *userctx,
	const struct ec_parsed_tk *var);

/**
 * Callback function type for evaluating a prefix-operator
 *
 * @param result
 *   On success, this pointer must be set by the user to point
 *   to a user structure describing the evaluated result.
 * @param userctx
 *   A user-defined context passed to all callback functions, which
 *   can be used to maintain a state or store global information.
 * @param operand
 *   The evaluated expression on which the operation should be applied.
 * @param var
 *   The parsed token referencing the operator.
 * @return
 *   0 on success (*result must be set, operand is freed),
 *   or -errno on error (*result is undefined, operand is not freed).
 */
typedef int (*ec_tk_expr_eval_pre_op_t)(
	void **result, void *userctx,
	void *operand,
	const struct ec_parsed_tk *operator);

typedef int (*ec_tk_expr_eval_post_op_t)(
	void **result, void *userctx,
	void *operand,
	const struct ec_parsed_tk *operator);

typedef int (*ec_tk_expr_eval_bin_op_t)(
	void **result, void *userctx,
	void *operand1,
	const struct ec_parsed_tk *operator,
	void *operand2);

typedef int (*ec_tk_expr_eval_parenthesis_t)(
	void **result, void *userctx,
	const struct ec_parsed_tk *open_paren,
	const struct ec_parsed_tk *close_paren,
	void * value);

typedef void (*ec_tk_expr_eval_free_t)(
	void *result, void *userctx);


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
	ec_tk_expr_eval_free_t eval_free;
};

int ec_tk_expr_eval(void **result, const struct ec_tk *tk,
	struct ec_parsed_tk *parsed, const struct ec_tk_expr_eval_ops *ops,
	void *userctx);

#endif
