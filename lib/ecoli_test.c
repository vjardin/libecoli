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

	p = ec_tk_parse(tk, input, 0);
	s = ec_parsed_tk_to_string(p);
	if (s == NULL && expected == NULL)
		ret = 0;
	else if (s != NULL && expected != NULL &&
		!strcmp(s, expected))
		ret = 0;

	if (expected == NULL && ret != 0)
		printf("tk should not match but matches <%s>\n", s);
	if (expected != NULL && ret != 0)
		printf("tk should match <%s> but matches <%s>\n",
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

	p = ec_tk_complete(tk, input, 0);
	s = ec_completed_tk_smallest_start(p);
	if (s == NULL && expected == NULL)
		ret = 0;
	else if (s != NULL && expected != NULL &&
		!strcmp(s, expected))
		ret = 0;

	if (expected == NULL && ret != 0)
		printf("tk should not complete but completes with <%s>\n", s);
	if (expected != NULL && ret != 0)
		printf("tk should complete with <%s> but completes with <%s>\n",
			expected, s);

	ec_completed_tk_free(p);

	return ret;
}

/* int ec_test_check_tk_complete_list(const struct ec_tk *tk, */
/*	const char *input, ...) */

int ec_test_all(void)
{
	struct ec_test *test;
	int ret = 0;

	// XXX register a new malloc to trac memleaks

	TAILQ_FOREACH(test, &test_list, next) {
		if (test->test() == 0) {
			printf("== test %-20s success\n", test->name);
		} else {
			printf("== test %-20s failed\n", test->name);
			ret = -1;
		}
	}

	return ret;
}
