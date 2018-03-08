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

#ifndef ECOLI_TEST_
#define ECOLI_TEST_

#include <sys/queue.h>

#include <ecoli_log.h>
#include <ecoli_node.h>

// XXX check if already exists?
#define EC_TEST_REGISTER(t)						\
	static void ec_test_init_##t(void);				\
	static void __attribute__((constructor, used))			\
	ec_test_init_##t(void)						\
	{								\
		 ec_test_register(&t);					\
	}

/**
 * Type of test function. Return 0 on success, -1 on error.
 */
typedef int (ec_test_t)(void);

TAILQ_HEAD(ec_test_list, ec_test);

/**
 * A structure describing a test case.
 */
struct ec_test {
	TAILQ_ENTRY(ec_test) next;  /**< Next in list. */
	const char *name;           /**< Test name. */
	ec_test_t *test;            /**< Test function. */
};

/**
 * Register a test case.
 *
 * @param test
 *   A pointer to a ec_test structure describing the test
 *   to be registered.
 */
void ec_test_register(struct ec_test *test);

int ec_test_all(void);
int ec_test_one(const char *name);

/* expected == -1 means no match */
int ec_test_check_parse(struct ec_node *node, int expected, ...);

#define EC_TEST_ERR(fmt, ...)						\
	EC_LOG(EC_LOG_ERR, "%s:%d: error: " fmt "\n",			\
		__FILE__, __LINE__, ##__VA_ARGS__);			\

/* XXX this is not an assert, it does not abort */
// XXX use it instead of ec_log to have the file:line
#define EC_TEST_ASSERT_STR(cond, fmt, ...)				\
	do {								\
		if (!(cond))						\
			EC_TEST_ERR("assert failure: (" #cond ") " fmt,	\
				##__VA_ARGS__);				\
	} while (0)

#define EC_TEST_ASSERT(cond) EC_TEST_ASSERT_STR(cond, "")

/* node, input, [expected1, expected2, ...] */
#define EC_TEST_CHECK_PARSE(node, args...) ({				\
	int ret_ = ec_test_check_parse(node, args, EC_NODE_ENDLIST);	\
	if (ret_)							\
		EC_TEST_ERR("parse test failed");			\
	ret_;								\
})

int ec_test_check_complete(struct ec_node *node, ...);

#define EC_TEST_CHECK_COMPLETE(node, args...) ({			\
	int ret_ = ec_test_check_complete(node, args);			\
	if (ret_)							\
		EC_TEST_ERR("complete test failed");			\
	ret_;								\
})

#endif
