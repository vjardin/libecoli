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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_tk.h>
#include <ecoli_tk_str.h>
#include <ecoli_tk_option.h>
#include <ecoli_tk_weakref.h>

struct ec_tk_weakref {
	struct ec_tk gen;
	struct ec_tk *child;
};

static struct ec_parsed_tk *ec_tk_weakref_parse(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_weakref *tk = (struct ec_tk_weakref *)gen_tk;

	return ec_tk_parse_tokens(tk->child, strvec);
}

static struct ec_completed_tk *ec_tk_weakref_complete(const struct ec_tk *gen_tk,
	const struct ec_strvec *strvec)
{
	struct ec_tk_weakref *tk = (struct ec_tk_weakref *)gen_tk;

	return ec_tk_complete_tokens(tk->child, strvec);
}

static struct ec_tk_type ec_tk_weakref_type = {
	.name = "weakref",
	.parse = ec_tk_weakref_parse,
	.complete = ec_tk_weakref_complete,
	.size = sizeof(struct ec_tk_weakref),
};

EC_TK_TYPE_REGISTER(ec_tk_weakref_type);

int ec_tk_weakref_set(struct ec_tk *gen_tk, struct ec_tk *child)
{
	struct ec_tk_weakref *tk = (struct ec_tk_weakref *)gen_tk;

	// XXX check tk type

	assert(tk != NULL);

	if (child == NULL)
		return -EINVAL;

	gen_tk->flags &= ~EC_TK_F_BUILT;

	tk->child = child;

	child->parent = gen_tk;
	TAILQ_INSERT_TAIL(&gen_tk->children, child, next); // XXX really needed?

	return 0;
}

struct ec_tk *ec_tk_weakref(const char *id, struct ec_tk *child)
{
	struct ec_tk *gen_tk = NULL;

	if (child == NULL)
		return NULL;

	gen_tk = __ec_tk_new(&ec_tk_weakref_type, id);
	if (gen_tk == NULL)
		return NULL;

	ec_tk_weakref_set(gen_tk, child);

	return gen_tk;
}

static int ec_tk_weakref_testcase(void)
{
	return 0;
}

static struct ec_test ec_tk_weakref_test = {
	.name = "tk_weakref",
	.test = ec_tk_weakref_testcase,
};

EC_TEST_REGISTER(ec_tk_weakref_test);
