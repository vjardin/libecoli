/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <ecoli_init.h>

static struct ec_init_list init_list = TAILQ_HEAD_INITIALIZER(init_list);

/* register an init function */
void ec_init_register(struct ec_init *init)
{
	TAILQ_INSERT_TAIL(&init_list, init, next);
}

int ec_init(void)
{
	struct ec_init *init;

	/* XXX sort list by priority */

	TAILQ_FOREACH(init, &init_list, next) {
		if (init->init() < 0)
			return -1;
	}

	return 0;
}
