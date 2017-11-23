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

#ifndef ECOLI_INIT_
#define ECOLI_INIT_

#include <sys/queue.h>

#include <ecoli_log.h>
#include <ecoli_node.h>

#define EC_INIT_REGISTER(t)						\
	static void ec_init_init_##t(void);				\
	static void __attribute__((constructor, used))			\
	ec_init_init_##t(void)						\
	{								\
		 ec_init_register(&t);					\
	}

/**
 * Type of init function. Return 0 on success, -1 on error.
 */
typedef int (ec_init_t)(void);

TAILQ_HEAD(ec_init_list, ec_init);

/**
 * A structure describing a test case.
 */
struct ec_init {
	TAILQ_ENTRY(ec_init) next;  /**< Next in list. */
	ec_init_t *init;            /**< Init function. */
	unsigned int priority;      /**< Priority (0=first, 99=last) */
};

/**
 * Register an initialization function.
 *
 * @param init
 *   A pointer to a ec_init structure to be registered.
 */
void ec_init_register(struct ec_init *test);

/**
 * Initialize ecoli library
 *
 * Must be called before any other function.
 *
 * @return
 *   0 on success, -1 on error (errno is set).
 */
int ec_init(void);

#endif
