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
#include <limits.h>
#include <execinfo.h>

#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_malloc.h>

EC_LOG_TYPE_REGISTER(main);

#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / \
		((size_t)(!(sizeof(x) % sizeof(0[x])))))

static int log_level = EC_LOG_INFO;
static int alloc_fail_proba = 0;
static int seed = 0;
static size_t alloc_success = 0;

static const char ec_short_options[] =
	"h"  /* help */
	"l:" /* log-level */
	"r:" /* random-alloc-fail */
	"s:" /* seed */
	;

#define EC_OPT_HELP "help"
#define EC_OPT_LOG_LEVEL "log-level"
#define EC_OPT_RANDOM_ALLOC_FAIL "random-alloc-fail"
#define EC_OPT_SEED "seed"

static const struct option ec_long_options[] = {
	{EC_OPT_HELP, 1, NULL, 'h'},
	{EC_OPT_LOG_LEVEL, 1, NULL, 'l'},
	{EC_OPT_RANDOM_ALLOC_FAIL, 1, NULL, 'r'},
	{EC_OPT_SEED, 1, NULL, 's'},
	{NULL, 0, NULL, 0}
};

static void usage(const char *prgname)
{
	printf("%s [options] [test1 test2 test3...]\n"
		"  -h\n"
		"  --"EC_OPT_HELP"\n"
		"      Show this help.\n"
		"  -l <level>\n"
		"  --"EC_OPT_LOG_LEVEL"=<level>\n"
		"      Set log level (0 = no log, 7 = verbose).\n"
		"  -r <probability>\n"
		"  --"EC_OPT_RANDOM_ALLOC_FAIL"=<probability>\n"
		"      Cause malloc to fail randomly. This helps to debug\n"
		"      leaks or crashes in error cases. The probability is\n"
		"      between 0 and 100.\n"
		"  -s <seed>\n"
		"  --seed=<seed>\n"
		"      Seeds the random number generator. Default is 0.\n"
		, prgname);
}

static int
parse_int(const char *s, int min, int max, int *ret, unsigned int base)
{
	char *end = NULL;
	long long n;

	n = strtoll(s, &end, base);
	if ((s[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;
	if (n < min)
		return -1;
	if (n > max)
		return -1;

	*ret = n;
	return 0;
}

static int parse_args(int argc, char **argv)
{
	int ret, opt;

	while ((opt = getopt_long(argc, argv, ec_short_options,
				ec_long_options, NULL)) != EOF) {

		switch (opt) {
		case 'h': /* help */
			usage(argv[0]);
			exit(0);

		case 'l': /* log-level */
			if (parse_int(optarg, EC_LOG_EMERG,
					EC_LOG_DEBUG, &log_level, 10) < 0) {
				printf("Invalid log value\n");
				usage(argv[0]);
				exit(1);
			}
			break;

		case 'r': /* random-alloc-fail */
			if (parse_int(optarg, 0, 100, &alloc_fail_proba,
					10) < 0) {
				printf("Invalid probability value\n");
				usage(argv[0]);
				exit(1);
			}
			break;

		case 's': /* seed */
			if (parse_int(optarg, 0, INT_MAX, &seed, 10) < 0) {
				printf("Invalid seed value\n");
				usage(argv[0]);
				exit(1);
			}
			break;

		default:
			usage(argv[0]);
			return -1;
		}
	}

	ret = optind - 1;
	optind = 1;

	return ret;
}

TAILQ_HEAD(debug_alloc_hdr_list, debug_alloc_hdr);
static struct debug_alloc_hdr_list debug_alloc_hdr_list =
	TAILQ_HEAD_INITIALIZER(debug_alloc_hdr_list);

#define STACK_SZ 16
struct debug_alloc_hdr {
	TAILQ_ENTRY(debug_alloc_hdr) next;
	const char *file;
	unsigned int line;
	size_t size;
	void *stack[STACK_SZ];
	int stacklen;
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


	if (alloc_fail_proba != 0 && (random() % 100) < alloc_fail_proba)
		hdr = NULL;
	else
		hdr = malloc(new_size);

	if (hdr == NULL) {
		ret = NULL;
	} else {
		hdr->file = file;
		hdr->line = line;
		hdr->size = size;
		hdr->stacklen = backtrace(hdr->stack, COUNT_OF(hdr->stack));
		hdr->cookie = 0x12345678;
		TAILQ_INSERT_TAIL(&debug_alloc_hdr_list, hdr, next);
		ret = hdr + 1;
		ftr = (struct debug_alloc_ftr *)(
			(char *)hdr + size + sizeof(*hdr));
		ftr->cookie = 0x87654321;
	}

	EC_LOG(EC_LOG_DEBUG, "%s:%d: info: malloc(%zd) -> %p\n",
		file, line, size, ret);

	if (ret)
		alloc_success++;
	return ret;
}

static void debug_free(void *ptr, const char *file, unsigned int line)
{
	struct debug_alloc_hdr *hdr, *h;
	struct debug_alloc_ftr *ftr;

	(void)file;
	(void)line;

	EC_LOG(EC_LOG_DEBUG, "%s:%d: info: free(%p)\n", file, line, ptr);

	if (ptr == NULL)
		return;

	hdr = (ptr - sizeof(*hdr));
	if (hdr->cookie != 0x12345678) {
		EC_LOG(EC_LOG_ERR, "%s:%d: error: free(%p): bad start cookie\n",
			file, line, ptr);
		abort();
	}

	ftr = (ptr + hdr->size);
	if (ftr->cookie != 0x87654321) {
		EC_LOG(EC_LOG_ERR, "%s:%d: error: free(%p): bad end cookie\n",
			file, line, ptr);
		abort();
	}

	TAILQ_FOREACH(h, &debug_alloc_hdr_list, next) {
		if (h == hdr)
			break;
	}

	if (h == NULL) {
		EC_LOG(EC_LOG_ERR, "%s:%d: error: free(%p): bad ptr\n",
			file, line, ptr);
		abort();
	}

	TAILQ_REMOVE(&debug_alloc_hdr_list, hdr, next);
	free(hdr);
}

static void *debug_realloc(void *ptr, size_t size, const char *file,
	unsigned int line)
{
	struct debug_alloc_hdr *hdr, *h;
	struct debug_alloc_ftr *ftr;
	size_t new_size = size + sizeof(*hdr) + sizeof(unsigned int);
	void *ret;

	if (ptr != NULL) {
		hdr =  (ptr - sizeof(*hdr));
		if (hdr->cookie != 0x12345678) {
			EC_LOG(EC_LOG_ERR,
				"%s:%d: error: realloc(%p): bad start cookie\n",
				file, line, ptr);
			abort();
		}

		ftr = (ptr + hdr->size);
		if (ftr->cookie != 0x87654321) {
			EC_LOG(EC_LOG_ERR,
				"%s:%d: error: realloc(%p): bad end cookie\n",
				file, line, ptr);
			abort();
		}

		TAILQ_FOREACH(h, &debug_alloc_hdr_list, next) {
			if (h == hdr)
				break;
		}

		if (h == NULL) {
			EC_LOG(EC_LOG_ERR, "%s:%d: error: realloc(%p): bad ptr\n",
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
		hdr->stacklen = backtrace(hdr->stack, COUNT_OF(hdr->stack));
		hdr->cookie = 0x12345678;
		TAILQ_INSERT_TAIL(&debug_alloc_hdr_list, hdr, next);
		ftr = (struct debug_alloc_ftr *)(
			(char *)hdr + size + sizeof(*hdr));
		ftr->cookie = 0x87654321;
	}

	EC_LOG(EC_LOG_DEBUG, "%s:%d: info: realloc(%p, %zd) -> %p\n",
		file, line, ptr, size, ret);

	if (ret)
		alloc_success++;
	return ret;
}

static int debug_alloc_dump_leaks(void)
{
	struct debug_alloc_hdr *hdr;
	int i;
	char **buffer;

	EC_LOG(EC_LOG_INFO, "%zd successful allocations\n", alloc_success);

	if (TAILQ_EMPTY(&debug_alloc_hdr_list))
		return 0;

	TAILQ_FOREACH(hdr, &debug_alloc_hdr_list, next) {
		EC_LOG(EC_LOG_ERR,
			"%s:%d: error: memory leak size=%zd ptr=%p\n",
			hdr->file, hdr->line, hdr->size, hdr + 1);
		buffer = backtrace_symbols(hdr->stack, hdr->stacklen);
		if (buffer == NULL) {
			for (i = 0; i < hdr->stacklen; i++)
				EC_LOG(EC_LOG_ERR, "  %p\n", hdr->stack[i]);
		} else {
			for (i = 0; i < hdr->stacklen; i++)
				EC_LOG(EC_LOG_ERR, "  %s\n",
					buffer ? buffer[i] : "unknown");
		}
		free(buffer);
	}

	EC_LOG(EC_LOG_ERR,
		"  missing static syms, use: addr2line -f -e <prog> <addr>\n");

	return -1;
}

static int debug_log(int type, unsigned int level, void *opaque,
		const char *str)
{
	(void)type;
	(void)opaque;

	if (level > (unsigned int)log_level)
		return 0;

	return printf("%s", str);
}

int main(int argc, char **argv)
{
	int i, ret = 0, leaks;

	ret = parse_args(argc, argv);
	if (ret < 0)
		return 1;

	argc -= ret;
	argv += ret;

	srandom(seed);

	if (0) ec_log_fct_register(debug_log, NULL);

	/* register a new malloc to track memleaks */
	TAILQ_INIT(&debug_alloc_hdr_list);
	if (ec_malloc_register(debug_malloc, debug_free, debug_realloc) < 0) {
		EC_LOG(EC_LOG_ERR, "cannot register new malloc\n");
		return -1;
	}

	ret = 0;
	if (argc <= 1) {
		ret = ec_test_all();
	} else {
		for (i = 1; i < argc; i++)
			ret |= ec_test_one(argv[i]);
	}

	ec_malloc_unregister();
	leaks = debug_alloc_dump_leaks();

	if (alloc_fail_proba == 0 && ret != 0) {
		printf("tests failed\n");
		return 1;
	} else if (alloc_fail_proba != 0 && leaks != 0) {
		printf("tests failed (memory leak)\n");
		return 1;
	}

	printf("\ntests ok\n");

	return 0;
}
