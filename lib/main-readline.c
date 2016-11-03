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

#include <readline/readline.h>
#include <readline/history.h>

#include <ecoli_tk_str.h>
#include <ecoli_tk_seq.h>
#include <ecoli_tk_space.h>
#include <ecoli_tk_or.h>

static struct ec_tk *commands;

/* Set to a character describing the type of completion being attempted by
   rl_complete_internal; available for use by application completion
   functions. */
extern int rl_completion_type;
/* Set to the last key used to invoke one of the completion functions */
extern int rl_completion_invoking_key;

int my_complete(int x, int y)
{
	(void)x;
	(void)y;

	return 0;
}

char *my_completion_entry(const char *s, int state)
{
	static struct ec_completed_tk *c;
	static const struct ec_completed_tk_elt *elt;

	(void)s;

	if (state == 0) {
		char *start;

		if (c != NULL)
			ec_completed_tk_free(c);

		start = strdup(rl_line_buffer);
		if (start == NULL)
			return NULL;
		start[rl_point] = '\0';

		c = ec_tk_complete(commands, start);
		ec_completed_tk_iter_start(c);
	}

	elt = ec_completed_tk_iter_next(c);
	if (elt == NULL)
		return NULL;

	return strdup(elt->full);
}

char **my_attempted_completion(const char *text, int start, int end)
{
	(void)start;
	(void)end;
	// XXX when it returns NULL, it completes with a file
	return rl_completion_matches(text, my_completion_entry);
}

int main(void)
{
	struct ec_parsed_tk *p;
//	const char *name;
	char *line;

	commands = ec_tk_seq_new_list(NULL,
		ec_tk_str_new(NULL, "hello"),
		ec_tk_space_new(NULL),
		ec_tk_or_new_list("name",
			ec_tk_str_new(NULL, "john"),
			ec_tk_str_new(NULL, "mike"),
			EC_TK_ENDLIST),
		EC_TK_ENDLIST);
	if (commands == NULL) {
		printf("cannot create token\n");
		return 1;
	}

	//rl_bind_key('\t', my_complete);

	//rl_completion_entry_function = my_completion_entry;
	rl_attempted_completion_function = my_attempted_completion;

	while (1) {
		line = readline("> ");
		if (line == NULL)
			break;

		// XXX need a "parse_all"
		p = ec_tk_parse(commands, line);
		ec_parsed_tk_dump(stdout, p);
		add_history(line);
		ec_parsed_tk_free(p);
	}


	ec_tk_free(commands);
	return 0;
}
