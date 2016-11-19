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
#include <ecoli_tk.h>

#define EC_REGISTER_TEST(t)						\
	static void ec_init_##t(void);					\
	static void __attribute__((constructor, used)) ec_init_##t(void) \
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

/* expected == -1 means no match */
int ec_test_check_tk_parse(const struct ec_tk *tk, int expected, ...);

#define TEST_ERR()							\
	ec_log(EC_LOG_ERR, "%s:%d: error: test failed\n",		\
		__FILE__, __LINE__);					\

#define EC_TEST_CHECK_TK_PARSE(tk, input, expected...) ({		\
	int ret_ = ec_test_check_tk_parse(tk, input, expected);		\
	if (ret_)							\
		TEST_ERR();						\
	ret_;								\
})

int ec_test_check_tk_complete(const struct ec_tk *tk, ...);

#define EC_TEST_CHECK_TK_COMPLETE(tk, args...) ({			\
	int ret_ = ec_test_check_tk_complete(tk, args);			\
	if (ret_)							\
		TEST_ERR();						\
	ret_;								\
})

#endif
