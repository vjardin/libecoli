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
size_t ec_vec_len(const struct ec_vec *vec);

#endif
