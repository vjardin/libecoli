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
#include <assert.h>
#include <getopt.h>

#include <ecoli_test.h>
#include <ecoli_malloc.h>

static const char ec_short_options[] =
	"h" /* help */
	;

enum {
	/* long options */
	EC_OPT_LONG_MIN_NUM = 256,
#define EC_OPT_HELP "help"
	EC_OPT_HELP_NUM,
};

static const struct option ec_long_options[] = {
	{EC_OPT_HELP, 1, NULL, EC_OPT_HELP_NUM},
	{NULL, 0, NULL, 0}
};

static void usage(const char *prgname)
{
	printf("%s [options]\n"
		   "  --"EC_OPT_HELP": show this help\n"
		, prgname);
}

static void parse_args(int argc, char **argv)
{
	int opt;

	while ((opt = getopt_long(argc, argv, ec_short_options,
				ec_long_options, NULL)) != EOF) {

		switch (opt) {
		case 'h': /* help */
		case EC_OPT_HELP_NUM:
			usage(argv[0]);
			exit(0);

		default:
			usage(argv[0]);
			exit(1);
		}
	}
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

struct debug_alloc_ftr {
	unsigned int cookie;
} __attribute__((packed));

static void *debug_malloc(size_t size, const char *file, unsigned int line)
{
	struct debug_alloc_hdr *hdr;
	struct debug_alloc_ftr *ftr;
	size_t new_size = size + sizeof(*hdr) + sizeof(*ftr);
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
		ftr = (struct debug_alloc_ftr *)(
			(char *)hdr + size + sizeof(*hdr));
		ftr->cookie = 0x12345678;
	}

	ec_log(EC_LOG_INFO, "%s:%d: info: malloc(%zd) -> %p\n",
		file, line, size, ret);

	return ret;
}

static void debug_free(void *ptr, const char *file, unsigned int line)
{
	struct debug_alloc_hdr *hdr, *h;
	struct debug_alloc_ftr *ftr;

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

	ftr = (ptr + hdr->size);
	if (ftr->cookie != 0x12345678) {
		ec_log(EC_LOG_ERR, "%s:%d: error: free(%p): bad end cookie\n",
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
	struct debug_alloc_hdr *hdr, *h;
	struct debug_alloc_ftr *ftr;
	size_t new_size = size + sizeof(*hdr) + sizeof(unsigned int);
	void *ret;

	if (ptr != NULL) {
		hdr =  (ptr - sizeof(*hdr));
		if (hdr->cookie != 0x12345678) {
			ec_log(EC_LOG_ERR,
				"%s:%d: error: realloc(%p): bad start cookie\n",
				file, line, ptr);
			abort();
		}

		ftr = (ptr + hdr->size);
		if (ftr->cookie != 0x12345678) {
			ec_log(EC_LOG_ERR,
				"%s:%d: error: realloc(%p): bad end cookie\n",
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
			ret = hdr + 1;
	}

	if (hdr != NULL) {
		hdr->file = file;
		hdr->line = line;
		hdr->size = size;
		hdr->cookie = 0x12345678;
		TAILQ_INSERT_TAIL(&debug_alloc_hdr_list, hdr, next);
		ftr = (struct debug_alloc_ftr *)(
			(char *)hdr + size + sizeof(*hdr));
		ftr->cookie = 0x12345678;
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

int main(int argc, char **argv)
{
	int ret;

	parse_args(argc, argv);

	TAILQ_INIT(&debug_alloc_hdr_list);
	/* register a new malloc to track memleaks */
	if (ec_malloc_register(debug_malloc, debug_free, debug_realloc) < 0) {
		ec_log(EC_LOG_ERR, "cannot register new malloc\n");
		return -1;
	}

	ret = ec_test_all();

	ec_malloc_unregister();
	debug_alloc_dump();

	if (ret != 0)
		printf("tests failed\n");

	return 0;
}
