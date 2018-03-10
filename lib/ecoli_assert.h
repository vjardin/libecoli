/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016, Olivier MATZ <zer0@droids-corp.org>
 */

/**
 * Assert API
 *
 * Helpers to check at runtime if a condition is true, and abort
 * (exit) otherwise.
 */

#ifndef ECOLI_ASSERT_
#define ECOLI_ASSERT_

#include <stdbool.h>

/**
 * Abort if the condition is false.
 *
 * If expression is false this macro will prints an error message to
 * standard error and terminates the program by calling abort(3).
 *
 * @param expr
 *   The expression to be checked.
 * @param args
 *   The format string, optionally followed by other arguments.
 */
#define ec_assert_print(expr, args...) \
	__ec_assert_print(expr, #expr, args)

/* internal */
void __ec_assert_print(bool expr, const char *expr_str,
		const char *format, ...);

#endif
