/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

/**
 * @defgroup test Test
 * @{
 *
 * @brief Helpers for unit tests
 */

#ifndef ECOLI_TEST_
#define ECOLI_TEST_

#include <sys/queue.h>

#include <ecoli_log.h>
#include <ecoli_utils.h>

struct ec_node;
enum ec_comp_type;

#define EC_TEST_REGISTER(t)						\
	static void ec_test_init_##t(void);				\
	static void __attribute__((constructor, used))			\
	ec_test_init_##t(void)						\
	{								\
		if (ec_test_register(&t) < 0)				\
			fprintf(stderr, "cannot register test %s\n",	\
				t.name);				\
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
 * @return
 *   0 on success, -1 on error (errno is set).
 */
int ec_test_register(struct ec_test *test);

int ec_test_all(void);
int ec_test_one(const char *name);

/* expected == -1 means no match */
int ec_test_check_parse(struct ec_node *node, int expected, ...);

#define EC_TEST_ERR(fmt, ...)						\
	EC_LOG(EC_LOG_ERR, "%s:%d: error: " fmt "\n",			\
		__FILE__, __LINE__, ##__VA_ARGS__);			\

#define EC_TEST_CHECK(cond, fmt, ...) ({				\
	int ret_ = 0;							\
	if (!(cond)) {							\
		EC_TEST_ERR("(" #cond ") is wrong. " fmt		\
			##__VA_ARGS__);					\
		ret_ = -1;						\
	}								\
	ret_;								\
})

/* node, input, [expected1, expected2, ...] */
#define EC_TEST_CHECK_PARSE(node, args...) ({				\
	int ret_ = ec_test_check_parse(node, args, EC_VA_END);	\
	if (ret_)							\
		EC_TEST_ERR("parse test failed");			\
	ret_;								\
})

int ec_test_check_complete(struct ec_node *node,
			enum ec_comp_type type, ...);

#define EC_TEST_CHECK_COMPLETE(node, args...) ({			\
	int ret_ = ec_test_check_complete(node, EC_COMP_FULL, args);	\
	if (ret_)							\
		EC_TEST_ERR("complete test failed");			\
	ret_;								\
})

#define EC_TEST_CHECK_COMPLETE_PARTIAL(node, args...) ({		\
	int ret_ = ec_test_check_complete(node, EC_COMP_PARTIAL, args);	\
	if (ret_)							\
		EC_TEST_ERR("complete test failed");			\
	ret_;								\
})

#endif

/** @} */
