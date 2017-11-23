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
 * MurmurHash3 is a hash implementation that was written by Austin Appleby, and
 * is placed in the public domain. The author hereby disclaims copyright to this
 * source code.
 */

#ifndef ECOLI_MURMURHASH_H_
#define ECOLI_MURMURHASH_H_

#include <stdint.h>

/** Hash rotation */
static inline uint32_t ec_murmurhash_rotl32(uint32_t x, int8_t r)
{
	return (x << r) | (x >> (32 - r));
}

/** Add 32-bit to the hash */
static inline uint32_t ec_murmurhash3_add32(uint32_t h, uint32_t data)
{
	data *= 0xcc9e2d51;
	data = ec_murmurhash_rotl32(data, 15);
	data *= 0x1b873593;
	h ^= data;
	return h;
}

/** Intermediate mix */
static inline uint32_t ec_murmurhash3_mix32(uint32_t h)
{
	h = ec_murmurhash_rotl32(h,13);
	h = h * 5 +0xe6546b64;
	return h;
}

/** Final mix: force all bits of a hash block to avalanche */
static inline uint32_t ec_murmurhash3_fmix32(uint32_t h)
{
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

/**
 * Calculate a 32-bit murmurhash3
 *
 * @param key
 *   The key (the unaligned variable-length array of bytes).
 * @param len
 *   The length of the key, counting by bytes.
 * @param seed
 *   Can be any 4-byte value initialization value.
 * @return
 *   A 32-bit hash.
 */
uint32_t ec_murmurhash3(const void *key, int len, uint32_t seed);

#endif /* ECOLI_MURMURHASH_H_ */
