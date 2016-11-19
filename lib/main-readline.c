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

#define _GNU_SOURCE /* for asprintf */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <ecoli_tk_str.h>
#include <ecoli_tk_seq.h>
#include <ecoli_tk_space.h>
#include <ecoli_tk_or.h>
#include <ecoli_tk_shlex.h>
#include <ecoli_tk_int.h>

static struct ec_tk *commands;

static char *my_completion_entry(const char *s, int state)
{
	static struct ec_completed_tk *c;
	static struct ec_completed_tk_iter *iter;
	static const struct ec_completed_tk_elt *elt;
	char *out_string;


	if (state == 0) {
		char *line;

		ec_completed_tk_free(c);

		line = strdup(rl_line_buffer);
		if (line == NULL)
			return NULL;
		line[rl_point] = '\0';

		c = ec_tk_complete(commands, line);
		free(line);
		if (c == NULL)
			return NULL;

		ec_completed_tk_iter_free(iter);
		iter = ec_completed_tk_iter_new(c, ITER_MATCH);
		if (iter == NULL)
			return NULL;
	}

	elt = ec_completed_tk_iter_next(iter);
	if (elt == NULL)
		return NULL;

	if (asprintf(&out_string, "%s%s", s, elt->add) < 0)
		return NULL;

	return out_string;
}

static char **my_attempted_completion(const char *text, int start, int end)
{
	(void)start;
	(void)end;

	/* remove default file completion */
	rl_attempted_completion_over = 1;

	return rl_completion_matches(text, my_completion_entry);
}


static int show_help(int ignore, int invoking_key)
{
	struct ec_completed_tk *c;
	char *line;

	(void)ignore;
	(void)invoking_key;

	printf("\nhelp:\n");
	line = strdup(rl_line_buffer);
	if (line == NULL)
		return 1;
	line[rl_point] = '\0';

	c = ec_tk_complete(commands, line);
	free(line);
	if (c == NULL)
		return 1;
	ec_completed_tk_dump(stdout, c);
	ec_completed_tk_free(c);

	rl_forced_update_display();

	return 0;
}

int main(void)
{
	struct ec_parsed_tk *p;
//	const char *name;
	char *line;

	rl_bind_key('?', show_help);

	commands = ec_tk_shlex_new(NULL,
		ec_tk_seq_new_list(NULL,
			ec_tk_str_new(NULL, "hello"),
			ec_tk_or_new_list("name",
				ec_tk_str_new(NULL, "john"),
				ec_tk_str_new(NULL, "johnny"),
				ec_tk_str_new(NULL, "mike"),
				ec_tk_int_new(NULL, 0, 10, 10),
				EC_TK_ENDLIST
			),
			EC_TK_ENDLIST
		)
	);
	if (commands == NULL) {
		printf("cannot create token\n");
		return 1;
	}

	rl_attempted_completion_function = my_attempted_completion;

	while (1) {
		line = readline("> ");
		if (line == NULL)
			break;

		p = ec_tk_parse(commands, line);
		ec_parsed_tk_dump(stdout, p);
		add_history(line);
		ec_parsed_tk_free(p);
	}


	ec_tk_free(commands);
	return 0;
}
