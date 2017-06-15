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

#include <ecoli_tk.h>
#include <ecoli_keyval.h>

#include <ecoli_tk_str.h>
#include <ecoli_tk_seq.h>
#include <ecoli_tk_space.h>
#include <ecoli_tk_or.h>
#include <ecoli_tk_sh_lex.h>
#include <ecoli_tk_int.h>
#include <ecoli_tk_option.h>
#include <ecoli_tk_cmd.h>

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
		iter = ec_completed_tk_iter_new(c, EC_MATCH);
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

/* this function builds the help string */
static char *get_tk_help(const struct ec_tk *tk)
{
	const struct ec_tk *tk2;
	char *help = NULL;
	char *tk_help = NULL;

	for (tk2 = tk; tk2 != NULL && tk_help == NULL; tk2 = ec_tk_parent(tk2))
		tk_help = ec_keyval_get(ec_tk_attrs(tk2), "help");

	if (tk_help == NULL)
		tk_help = "";

	if (asprintf(&help, "%-20s %s", ec_tk_desc(tk), tk_help) < 0)
		return NULL;

	return help;
}

static int show_help(int ignore, int invoking_key)
{
	const struct ec_completed_tk_elt *elt;
	struct ec_completed_tk_iter *iter;
	struct ec_completed_tk *c;
	char *line;
	unsigned int count, i;
	char **helps = NULL;

	(void)ignore;
	(void)invoking_key;

	line = strdup(rl_line_buffer);
	if (line == NULL)
		return 1;
	line[rl_point] = '\0';

	c = ec_tk_complete(commands, line);
	free(line);
	if (c == NULL)
		return 1;
	//ec_completed_tk_dump(stdout, c);

	count = ec_completed_tk_count(c, EC_MATCH | EC_NO_MATCH);
	helps = calloc(count + 1, sizeof(char *));
	if (helps == NULL)
		return 1;

	iter = ec_completed_tk_iter_new(c, EC_MATCH | EC_NO_MATCH);
	if (iter == NULL)
		goto fail;

	/* strangely, rl_display_match_list() expects first index at 1 */
	for (i = 1, elt = ec_completed_tk_iter_next(iter);
	     i <= count && elt != NULL;
	     i++, elt = ec_completed_tk_iter_next(iter)) {
		helps[i] = get_tk_help(elt->tk);
	}

	ec_completed_tk_free(c);

	rl_display_match_list(helps, count, 1000);

	rl_forced_update_display();

	return 0;

fail:
	free(helps);
	// free helps[n] XXX
	return 1;
}

static int create_commands(void)
{
	struct ec_tk *cmdlist = NULL, *cmd = NULL;

	cmdlist = ec_tk_new("or", NULL);
	if (cmdlist == NULL)
		goto fail;

	cmd = EC_TK_SEQ(NULL,
		ec_tk_str(NULL, "hello"),
		EC_TK_OR("name",
			ec_tk_str(NULL, "john"),
			ec_tk_str(NULL, "johnny"),
			ec_tk_str(NULL, "mike")
		),
		ec_tk_option_new(NULL, ec_tk_int("int", 0, 10, 10))
	);
	if (cmd == NULL)
		goto fail;
	ec_keyval_set(ec_tk_attrs(cmd), "help",
		"say hello to someone several times", NULL);
	ec_keyval_set(ec_tk_attrs(ec_tk_find(cmd, "name")),
		"help", "the name of the person", NULL);
	ec_keyval_set(ec_tk_attrs(ec_tk_find(cmd, "int")),
		"help", "an integer", NULL);
	if (ec_tk_or_add(cmdlist, cmd) < 0)
		goto fail;

#if 0
	cmd = EC_TK_CMD(NULL, "good morning john|johnny|mike [count]",
			ec_tk_int("count", 0, 10, 10));
	if (cmd == NULL)
		goto fail;
	ec_keyval_set(ec_tk_attrs(cmd), "help",
		"say good morning to someone several times", NULL);
	if (ec_tk_or_add(cmdlist, cmd) < 0)
		goto fail;
#endif

	cmd = EC_TK_SEQ(NULL,
		ec_tk_str(NULL, "bye")
	);
	ec_keyval_set(ec_tk_attrs(cmd), "help", "say bye to someone", NULL);
	if (ec_tk_or_add(cmdlist, cmd) < 0)
		goto fail;

	commands = ec_tk_sh_lex_new(NULL, cmdlist);
	if (commands == NULL)
		goto fail;

	return 0;

 fail:
	fprintf(stderr, "cannot initialize tokens\n");
	ec_tk_free(cmdlist);
	return -1;
}

int main(void)
{
	struct ec_parsed_tk *p;
//	const char *name;
	char *line;


	if (create_commands() < 0)
		return 1;

	rl_bind_key('?', show_help);
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
