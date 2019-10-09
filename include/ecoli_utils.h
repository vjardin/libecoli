/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018, Olivier MATZ <zer0@droids-corp.org>
 */

/**
 * @defgroup utils Utils
 * @{
 *
 * @brief Misc utils
 */


#ifndef ECOLI_UTILS_
#define ECOLI_UTILS_

/**
 * Cast a variable into a type, ensuring its initial type first
 */
#define EC_CAST(x, old_type, new_type) ({	\
	old_type __x = (x);			\
	(new_type)__x;				\
	})

#endif

/** @} */
