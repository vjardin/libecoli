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
#include <assert.h>

#include <ecoli_log.h>
#include <ecoli_malloc.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_parsed.h>
#include <ecoli_completed.h>
#include <ecoli_parsed.h>

static struct ec_test_list test_list = TAILQ_HEAD_INITIALIZER(test_list);

EC_LOG_TYPE_REGISTER(test);

/* register a test case */
void ec_test_register(struct ec_test *test)
{
	TAILQ_INSERT_TAIL(&test_list, test, next);
	// XXX check if already exist, like for type
}

int ec_test_check_parse(struct ec_node *tk, int expected, ...)
{
	struct ec_parsed *p;
	struct ec_strvec *vec = NULL;
	const char *s;
	int ret = -1, match;
	va_list ap;

	va_start(ap, expected);

	/* build a string vector */
	vec = ec_strvec();
	if (vec == NULL)
		goto out;

	for (s = va_arg(ap, const char *);
	     s != EC_NODE_ENDLIST;
	     s = va_arg(ap, const char *)) {
		if (s == NULL)
			goto out;

		if (ec_strvec_add(vec, s) < 0)
			goto out;
	}

	p = ec_node_parse_strvec(tk, vec);
	if (p == NULL) {
		EC_LOG(EC_LOG_ERR, "parsed is NULL\n");
	}
	if (ec_parsed_matches(p))
		match = ec_parsed_len(p);
	else
		match = -1;
	if (expected == match) {
		ret = 0;
	} else {
		EC_LOG(EC_LOG_ERR,
			"tk parsed len (%d) does not match expected (%d)\n",
			match, expected);
	}

	ec_parsed_free(p);

out:
	ec_strvec_free(vec);
	va_end(ap);
	return ret;
}

int ec_test_check_complete(struct ec_node *tk, enum ec_completed_type type, ...)
{
	struct ec_completed *c = NULL;
	struct ec_strvec *vec = NULL;
	const char *s;
	int ret = 0;
	unsigned int count = 0;
	va_list ap;

	va_start(ap, type);

	/* build a string vector */
	vec = ec_strvec();
	if (vec == NULL)
		goto out;

	for (s = va_arg(ap, const char *);
	     s != EC_NODE_ENDLIST;
	     s = va_arg(ap, const char *)) {
		if (s == NULL)
			goto out;

		if (ec_strvec_add(vec, s) < 0)
			goto out;
	}

	c = ec_node_complete_strvec(tk, vec);
	if (c == NULL) {
		ret = -1;
		goto out;
	}

	/* for each expected completion, check it is there */
	for (s = va_arg(ap, const char *);
	     s != EC_NODE_ENDLIST;
	     s = va_arg(ap, const char *)) {
		struct ec_completed_iter *iter;
		const struct ec_completed_item *item;

		if (s == NULL) {
			ret = -1;
			goto out;
		}

		count++;

		/* only check matching completions */
		iter = ec_completed_iter(c, type);
		while ((item = ec_completed_iter_next(iter)) != NULL) {
			const char *str = ec_completed_item_get_str(item);
			if (str != NULL && strcmp(str, s) == 0)
				break;
		}

		if (item == NULL) {
			EC_LOG(EC_LOG_ERR,
				"completion <%s> not in list\n", s);
			ret = -1;
		}
		ec_completed_iter_free(iter);
	}

	/* check if we have more completions (or less) than expected */
	if (count != ec_completed_count(c, type)) {
		EC_LOG(EC_LOG_ERR,
			"nb_completion (%d) does not match (%d)\n",
			count, ec_completed_count(c, type));
		ec_completed_dump(stdout, c);
		ret = -1;
	}

out:
	ec_strvec_free(vec);
	ec_completed_free(c);
	va_end(ap);
	return ret;
}

static int launch_test(const char *name)
{
	struct ec_test *test;
	int ret = 0;
	unsigned int count = 0;

	TAILQ_FOREACH(test, &test_list, next) {
		if (name != NULL && strcmp(name, test->name))
			continue;

		EC_LOG(EC_LOG_INFO, "== starting test %-20s\n",
			test->name);

		count++;
		if (test->test() == 0) {
			EC_LOG(EC_LOG_INFO,
				"== test %-20s success\n",
				test->name);
		} else {
			EC_LOG(EC_LOG_INFO,
				"== test %-20s failed\n",
				test->name);
			ret = -1;
		}
	}

	if (name != NULL && count == 0) {
		EC_LOG(EC_LOG_WARNING,
			"== test %s not found\n", name);
		ret = -1;
	}

	return ret;
}

int ec_test_all(void)
{
	return launch_test(NULL);
}

int ec_test_one(const char *name)
{
	return launch_test(name);
}
