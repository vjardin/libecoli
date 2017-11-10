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
 * Vectors of strings.
 *
 * The ec_strvec API provide helpers to manipulate string vectors.
 * When duplicating vectors, the strings are not duplicated in memory,
 * a reference counter is used.
 */

#ifndef ECOLI_STRVEC_
#define ECOLI_STRVEC_

#include <stdio.h>

/**
 * Allocate a new empty string vector.
 *
 * @return
 *   The new strvec object, or NULL on error (errno is set).
 */
struct ec_strvec *ec_strvec(void);

/**
 * Add a string in a vector.
 *
 * @param strvec
 *   The pointer to the string vector.
 * @param s
 *   The string to be added at the end of the vector.
 * @return
 *   0 on success or -1 on error (errno is set).
 */
int ec_strvec_add(struct ec_strvec *strvec, const char *s);

/**
 * Duplicate a string vector.
 *
 * @param strvec
 *   The pointer to the string vector.
 * @return
 *   The duplicated strvec object, or NULL on error (errno is set).
 */
struct ec_strvec *ec_strvec_dup(const struct ec_strvec *strvec);

/**
 * Duplicate a part of a string vector.
 *
 * @param strvec
 *   The pointer to the string vector.
 * @param off
 *   The index of the first string to duplicate.
 * @param
 *   The number of strings to duplicate.
 * @return
 *   The duplicated strvec object, or NULL on error (errno is set).
 */
struct ec_strvec *ec_strvec_ndup(const struct ec_strvec *strvec,
	size_t off, size_t len);

/**
 * Free a string vector.
 *
 * @param strvec
 *   The pointer to the string vector.
 */
void ec_strvec_free(struct ec_strvec *strvec);

/**
 * Get the length of a string vector.
 *
 * @param strvec
 *   The pointer to the string vector.
 * @return
 *   The length of the vector.
 */
size_t ec_strvec_len(const struct ec_strvec *strvec);

/**
 * Get a string element from a vector.
 *
 * @param strvec
 *   The pointer to the string vector.
 * @param idx
 *   The index of the string to get.
 * @return
 *   The string stored at given index, or NULL on error (strvec is NULL
 *   or invalid index).
 */
const char *ec_strvec_val(const struct ec_strvec *strvec, size_t idx);

/**
 * Dump a string vector.
 *
 * @param out
 *   The stream where the dump is sent.
 * @param strvec
 *   The pointer to the string vector.
 */
void ec_strvec_dump(FILE *out, const struct ec_strvec *strvec);

#endif
