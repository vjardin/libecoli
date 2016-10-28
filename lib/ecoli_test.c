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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ecoli_log.h>
#include <ecoli_malloc.h>
#include <ecoli_test.h>
#include <ecoli_tk.h>

static struct ec_test_list test_list = TAILQ_HEAD_INITIALIZER(test_list);

/* register a driver */
void ec_test_register(struct ec_test *test)
{
	TAILQ_INSERT_TAIL(&test_list, test, next);
}

int ec_test_check_tk_parse(const struct ec_tk *tk, const char *input,
	const char *expected)
{
	struct ec_parsed_tk *p;
	const char *s;
	int ret = -1;

	p = ec_tk_parse(tk, input);
	s = ec_parsed_tk_to_string(p);
	if (s == NULL && expected == NULL)
		ret = 0;
	else if (s != NULL && expected != NULL &&
		!strcmp(s, expected))
		ret = 0;

	if (expected == NULL && ret != 0)
		ec_log(EC_LOG_ERR, "tk should not match but matches <%s>\n", s);
	if (expected != NULL && ret != 0)
		ec_log(EC_LOG_ERR, "tk should match <%s> but matches <%s>\n",
			expected, s);

	ec_parsed_tk_free(p);

	return ret;
}

int ec_test_check_tk_complete(const struct ec_tk *tk, const char *input,
	const char *expected)
{
	struct ec_completed_tk *p;
	const char *s;
	int ret = -1;

	p = ec_tk_complete(tk, input);
	s = ec_completed_tk_smallest_start(p);
	if (s == NULL && expected == NULL)
		ret = 0;
	else if (s != NULL && expected != NULL &&
		!strcmp(s, expected))
		ret = 0;

	if (expected == NULL && ret != 0)
		ec_log(EC_LOG_ERR,
			"tk should not complete but completes with <%s>\n", s);
	if (expected != NULL && ret != 0)
		ec_log(EC_LOG_ERR,
			"tk should complete with <%s> but completes with <%s>\n",
			expected, s);

	ec_completed_tk_free(p);

	return ret;
}

TAILQ_HEAD(debug_alloc_hdr_list, debug_alloc_hdr);
static struct debug_alloc_hdr_list debug_alloc_hdr_list =
	TAILQ_HEAD_INITIALIZER(debug_alloc_hdr_list);

struct debug_alloc_hdr {
	TAILQ_ENTRY(debug_alloc_hdr) next;
	const char *file;
	unsigned int line;
	size_t size;
	unsigned int cookie;
};

static void *debug_malloc(size_t size, const char *file, unsigned int line)
{
	struct debug_alloc_hdr *hdr;
	size_t new_size = size + sizeof(*hdr) + sizeof(unsigned int);
	void *ret;

	hdr = malloc(new_size);
	if (hdr == NULL) {
		ret = NULL;
	} else {
		hdr->file = file;
		hdr->line = line;
		hdr->size = size;
		hdr->cookie = 0x12345678;
		TAILQ_INSERT_TAIL(&debug_alloc_hdr_list, hdr, next);
		ret = hdr + 1;
	}

	ec_log(EC_LOG_INFO, "%s:%d: info: malloc(%zd) -> %p\n",
		file, line, size, ret);

	return ret;
}

static void debug_free(void *ptr, const char *file, unsigned int line)
{
	struct debug_alloc_hdr *hdr, *h;

	(void)file;
	(void)line;

	ec_log(EC_LOG_INFO, "%s:%d: info: free(%p)\n", file, line, ptr);

	if (ptr == NULL)
		return;

	hdr = (ptr - sizeof(*hdr));
	if (hdr->cookie != 0x12345678) {
		ec_log(EC_LOG_ERR, "%s:%d: error: free(%p): bad start cookie\n",
			file, line, ptr);
		abort();
	}

	TAILQ_FOREACH(h, &debug_alloc_hdr_list, next) {
		if (h == hdr)
			break;
	}

	if (h == NULL) {
		ec_log(EC_LOG_ERR, "%s:%d: error: free(%p): bad ptr\n",
			file, line, ptr);
		abort();
	}

	TAILQ_REMOVE(&debug_alloc_hdr_list, hdr, next);
	free(hdr);
}

void *debug_realloc(void *ptr, size_t size, const char *file, unsigned int line)
{
	struct debug_alloc_hdr *hdr = (ptr - sizeof(*hdr));
	struct debug_alloc_hdr *h;
	size_t new_size = size + sizeof(*hdr) + sizeof(unsigned int);
	void *ret;

	if (ptr != NULL) {
		if (hdr->cookie != 0x12345678) {
			ec_log(EC_LOG_ERR, "%s:%d: error: realloc(%p): bad start cookie\n",
				file, line, ptr);
			abort();
		}

		TAILQ_FOREACH(h, &debug_alloc_hdr_list, next) {
			if (h == hdr)
				break;
		}

		if (h == NULL) {
			ec_log(EC_LOG_ERR, "%s:%d: error: realloc(%p): bad ptr\n",
				file, line, ptr);
			abort();
		}

		TAILQ_REMOVE(&debug_alloc_hdr_list, h, next);
		hdr = realloc(hdr, new_size);
		if (hdr == NULL) {
			TAILQ_INSERT_TAIL(&debug_alloc_hdr_list, h, next);
			ret = NULL;
		} else {
			ret = hdr + 1;
		}
	} else {
		hdr = realloc(NULL, new_size);
		if (hdr == NULL)
			ret = NULL;
		else
			ret= hdr + 1;
	}

	if (hdr != NULL) {
		hdr->file = file;
		hdr->line = line;
		hdr->size = size;
		hdr->cookie = 0x12345678;
		TAILQ_INSERT_TAIL(&debug_alloc_hdr_list, hdr, next);
	}

	ec_log(EC_LOG_INFO, "%s:%d: info: realloc(%p, %zd) -> %p\n",
		file, line, ptr, size, ret);

	return ret;
}

void debug_alloc_dump(void)
{
	struct debug_alloc_hdr *hdr;

	TAILQ_FOREACH(hdr, &debug_alloc_hdr_list, next) {
		ec_log(EC_LOG_ERR, "%s:%d: error: memory leak size=%zd ptr=%p\n",
			hdr->file, hdr->line, hdr->size, hdr + 1);
	}
}

/* XXX todo */
/* int ec_test_check_tk_complete_list(const struct ec_tk *tk, */
/*	const char *input, ...) */

int ec_test_all(void)
{
	struct ec_test *test;
	int ret = 0;

	TAILQ_INIT(&debug_alloc_hdr_list);

	/* register a new malloc to trac memleaks */
	if (ec_malloc_register(debug_malloc, debug_free, debug_realloc) < 0) {
		ec_log(EC_LOG_ERR, "cannot register new malloc\n");
		return -1;
	}

	TAILQ_FOREACH(test, &test_list, next) {
		if (test->test() == 0) {
			ec_log(EC_LOG_INFO, "== test %-20s success\n",
				test->name);
		} else {
			ec_log(EC_LOG_INFO, "== test %-20s failed\n",
				test->name);
			ret = -1;
		}
	}

	ec_malloc_unregister();
	debug_alloc_dump();

	return ret;
}
