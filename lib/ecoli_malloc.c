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


#include <ecoli_malloc.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct ec_malloc_handler ec_malloc_handler;

int ec_malloc_register(ec_malloc_t usr_malloc, ec_free_t usr_free,
	ec_realloc_t usr_realloc)
{
	if (usr_malloc == NULL || usr_free == NULL || usr_realloc == NULL)
		return -1;

	ec_malloc_handler.malloc = usr_malloc;
	ec_malloc_handler.free = usr_free;
	ec_malloc_handler.realloc = usr_realloc;

	return 0;
}

void ec_malloc_unregister(void)
{
	ec_malloc_handler.malloc = NULL;
	ec_malloc_handler.free = NULL;
	ec_malloc_handler.realloc = NULL;
}

void *__ec_malloc(size_t size, const char *file, unsigned int line)
{
	return ec_malloc_handler.malloc(size, file, line);
}

void __ec_free(void *ptr, const char *file, unsigned int line)
{
	ec_malloc_handler.free(ptr, file, line);
}

void *__ec_calloc(size_t nmemb, size_t size, const char *file,
	unsigned int line)
{
	void *ptr;
	size_t total;

	total = size * nmemb;
	if (nmemb != 0 && size != (total / nmemb)) {
		errno = ENOMEM;
		return NULL;
	}

	ptr = __ec_malloc(total, file, line);
	if (ptr == NULL)
		return NULL;

	memset(ptr, 0, total);
	return ptr;
}

void *__ec_realloc(void *ptr, size_t size, const char *file, unsigned int line)
{
	return ec_malloc_handler.realloc(ptr, size, file, line);
}

char *__ec_strdup(const char *s, const char *file, unsigned int line)
{
	size_t sz = strlen(s) + 1;
	char *s2;

	s2 = __ec_malloc(sz, file, line);
	if (s2 == NULL)
		return NULL;

	memcpy(s2, s, sz);

	return s2;
}

char *__ec_strndup(const char *s, size_t n, const char *file, unsigned int line)
{
	size_t sz = strnlen(s, n);
	char *s2;

	s2 = __ec_malloc(sz + 1, file, line);
	if (s2 == NULL)
		return NULL;

	memcpy(s2, s, sz);
	s2[sz] = '\0';

	return s2;
}
