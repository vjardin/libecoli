/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

/**
 * Vectors of objects.
 *
 * The ec_vec API provide helpers to manipulate vectors of objects
 * of any kind.
 */

#ifndef ECOLI_VEC_
#define ECOLI_VEC_

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>

/* if NULL, default does nothing */
typedef void (*ec_vec_elt_free_t)(void *ptr);

/* if NULL, default is:
 * memcpy(dst, src, vec->elt_size)
 */
typedef void (*ec_vec_elt_copy_t)(void *dst, void *src);

struct ec_vec *ec_vec(size_t elt_size, size_t size,
		ec_vec_elt_copy_t copy, ec_vec_elt_free_t free);
int ec_vec_add_by_ref(struct ec_vec *vec, void *ptr);

int ec_vec_add_ptr(struct ec_vec *vec, void *elt);
int ec_vec_add_u8(struct ec_vec *vec, uint8_t elt);
int ec_vec_add_u16(struct ec_vec *vec, uint16_t elt);
int ec_vec_add_u32(struct ec_vec *vec, uint32_t elt);
int ec_vec_add_u64(struct ec_vec *vec, uint64_t elt);

int ec_vec_get(void *ptr, const struct ec_vec *vec, size_t idx);

struct ec_vec *ec_vec_dup(const struct ec_vec *vec);
struct ec_vec *ec_vec_ndup(const struct ec_vec *vec,
	size_t off, size_t len);
void ec_vec_free(struct ec_vec *vec);

__attribute__((pure))
size_t ec_vec_len(const struct ec_vec *vec);

#endif
