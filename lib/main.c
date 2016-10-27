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

#include <ecoli_test.h>
#include <ecoli_tk_str.h>
#include <ecoli_tk_seq.h>
#include <ecoli_tk_space.h>
#include <ecoli_tk_or.h>

static void test(void)
{
	struct ec_tk *seq;
	struct ec_parsed_tk *p;
	const char *name;

	seq = ec_tk_seq_new_list(NULL,
		ec_tk_str_new(NULL, "hello"),
		ec_tk_space_new(NULL),
		ec_tk_or_new_list("name",
			ec_tk_str_new(NULL, "john"),
			ec_tk_str_new(NULL, "mike"),
			EC_TK_ENDLIST),
		EC_TK_ENDLIST);
	if (seq == NULL) {
		printf("cannot create token\n");
		return;
	}

	/* ok */
	p = ec_tk_parse(seq, "hello  mike", 0);
	ec_parsed_tk_dump(p);
	name = ec_parsed_tk_to_string(ec_parsed_tk_find_first(p, "name"));
	printf("parsed with name=%s\n", name);
	ec_parsed_tk_free(p);

	/* ko */
	p = ec_tk_parse(seq, "hello robert", 0);
	ec_parsed_tk_dump(p);
	name = ec_parsed_tk_to_string(ec_parsed_tk_find_first(p, "name"));
	printf("parsed with name=%s\n", name);
	ec_parsed_tk_free(p);

	ec_tk_free(seq);
}

int main(void)
{
	ec_test_all();

	test();

	return 0;
}
