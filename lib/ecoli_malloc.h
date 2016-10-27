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

#ifndef ECOLI_MALLOC_
#define ECOLI_MALLOC_

#include <sys/types.h>

typedef void *(*ec_malloc_t)(size_t size);
typedef void (*ec_free_t)(void *ptr);
typedef void *(*ec_realloc_t)(void *ptr, size_t size);

struct ec_malloc_handler {
	ec_malloc_t malloc;
	ec_free_t free;
	ec_realloc_t realloc;
};

extern struct ec_malloc_handler ec_malloc_handler;

int ec_malloc_register(ec_malloc_t usr_malloc, ec_free_t usr_free,
	ec_realloc_t usr_realloc);

/* internal */
void *__ec_malloc(size_t size);
void __ec_free(void *ptr);
void *__ec_calloc(size_t nmemb, size_t size);
void *__ec_realloc(void *ptr, size_t size);
char *__ec_strdup(const char *s);
char *__ec_strndup(const char *s, size_t n);

/* we use macros here to ensure the file/line stay correct when
 * debugging the standard malloc with valgrind */

#define ec_malloc(sz) ({				\
	void *ret_;					\
	if (ec_malloc_handler.malloc == NULL)		\
		ret_ = malloc(sz);			\
	else						\
		ret_ = __ec_malloc(sz);			\
	ret_;						\
	})

#define ec_free(ptr) ({					\
	if (ec_malloc_handler.free == NULL)		\
		free(ptr);				\
	else						\
		__ec_free(ptr);				\
	})

#define ec_realloc(ptr, sz) ({				\
	void *ret_;					\
	if (ec_malloc_handler.realloc == NULL)		\
		ret_ = realloc(ptr, sz);		\
	else						\
		ret_ = __ec_realloc(ptr, sz);		\
	ret_;						\
	})

#define ec_calloc(n, sz) ({				\
	void *ret_;					\
	if (ec_malloc_handler.malloc == NULL)		\
		ret_ = calloc(n, sz);			\
	else						\
		ret_ = __ec_calloc(n, sz);		\
	ret_;						\
	})

#define ec_strdup(s) ({					\
	void *ret_;					\
	if (ec_malloc_handler.malloc == NULL)		\
		ret_ = strdup(s);			\
	else						\
		ret_ = __ec_strdup(s);			\
	ret_;						\
	})

#define ec_strndup(s, n) ({				\
	void *ret_;					\
	if (ec_malloc_handler.malloc == NULL)		\
		ret_ = strndup(s, n);			\
	else						\
		ret_ = __ec_strndup(s, n);		\
	ret_;						\
	})


#endif
