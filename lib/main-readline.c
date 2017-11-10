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

#include <ecoli_node.h>
#include <ecoli_parsed.h>
#include <ecoli_completed.h>
#include <ecoli_keyval.h>
#include <ecoli_node_str.h>
#include <ecoli_node_seq.h>
#include <ecoli_node_space.h>
#include <ecoli_node_or.h>
#include <ecoli_node_sh_lex.h>
#include <ecoli_node_int.h>
#include <ecoli_node_option.h>
#include <ecoli_node_cmd.h>
#include <ecoli_node_many.h>
#include <ecoli_node_once.h>
#include <ecoli_node_file.h>

static struct ec_node *commands;

static char *my_completion_entry(const char *s, int state)
{
	static struct ec_completed *c;
	static struct ec_completed_iter *iter;
	const struct ec_completed_item *item;

	(void)s;

	/* don't append a quote */
	rl_completion_suppress_quote = 1;
	rl_basic_quote_characters = "";

	if (state == 0) {
		char *line;

		ec_completed_free(c);

		line = strdup(rl_line_buffer);
		if (line == NULL)
			return NULL;
		line[rl_point] = '\0';

		c = ec_node_complete(commands, line);
		free(line);
		if (c == NULL)
			return NULL;

		ec_completed_iter_free(iter);
		iter = ec_completed_iter(c, EC_MATCH | EC_PARTIAL_MATCH);
		if (iter == NULL)
			return NULL;
	}

	item = ec_completed_iter_next(iter);
	if (item == NULL)
		return NULL;

	if (c->count_match == 1) {

		/* don't add the trailing space for partial completions */
		if (state == 0) {
			if (item->type == EC_MATCH)
				rl_completion_suppress_append = 0;
			else
				rl_completion_suppress_append = 1;
		}

		return strdup(item->str);
	} else if (rl_completion_type == '?') {
		/* on second try only show the display part */
		return strdup(item->display);
	}

	return strdup(item->str);
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
static char *get_node_help(const struct ec_completed_node *compnode)
{
	const struct ec_completed_item *item;
	const struct ec_node *node;
	char *help = NULL;
	const char *node_help = NULL;
	const char *node_desc = NULL;
	size_t i;

	/* Since we display only one help per node, only look at the first item
	 * to get the path. The objective is to retrieve the most precise
	 * help for this node. */
	item = TAILQ_FIRST(&compnode->items);
	for (i = 0; i < item->pathlen; i++) {
		node = item->path[i];
		if (node_help == NULL)
			node_help = ec_keyval_get(ec_node_attrs(node), "help");
		if (node_desc == NULL)
			node_desc = ec_node_desc(node);
	}

	if (node_help == NULL)
		node_help = "";
	if (node_desc == NULL)
		return NULL;

	if (asprintf(&help, "%-20s %s", node_desc, node_help) < 0)
		return NULL;

	return help;
}

static int show_help(int ignore, int invoking_key)
{
	const struct ec_completed_node *compnode;
	struct ec_completed *c;
	struct ec_parsed *p;
	char *line;
	unsigned int count, i;
	char **helps = NULL;
	int match = 0;

	(void)ignore;
	(void)invoking_key;

	line = strdup(rl_line_buffer);
	if (line == NULL)
		return 1;

	/* check if the current line matches */
	p = ec_node_parse(commands, line);
	if (ec_parsed_matches(p))
		match = 1;
	ec_parsed_free(p);

	/* complete at current cursor position */
	line[rl_point] = '\0';
	c = ec_node_complete(commands, line);
	//ec_completed_dump(stdout, c);
	free(line);
	if (c == NULL)
		return 1;

	/* let's display one contextual help per node */
	count = 0;
	TAILQ_FOREACH(compnode, &c->nodes, next) {
		if (TAILQ_EMPTY(&compnode->items))
			continue;
		count++;
	}

	helps = calloc(count + match + 1, sizeof(char *));
	if (helps == NULL)
		return 1;

	if (match)
		helps[1] = "<return>";

	/* strangely, rl_display_match_list() expects first index at 1 */
	i = match + 1;
	TAILQ_FOREACH(compnode, &c->nodes, next) {
		if (TAILQ_EMPTY(&compnode->items))
			continue;
		helps[i++] = get_node_help(compnode);
	}

	ec_completed_free(c);

	rl_display_match_list(helps, count + match, 1000); /* XXX 1000 */

	rl_forced_update_display();

	return 0;

	// free helps[n] XXX on error ?
}

static int create_commands(void)
{
	struct ec_node *cmdlist = NULL, *cmd = NULL;

	cmdlist = ec_node("or", NULL);
	if (cmdlist == NULL)
		goto fail;


	cmd = EC_NODE_SEQ(NULL,
		ec_node_str(NULL, "hello"),
		EC_NODE_OR("name",
			ec_node_str("john", "john"),
			ec_node_str(NULL, "johnny"),
			ec_node_str(NULL, "mike")
		),
		ec_node_option(NULL, ec_node_int("int", 0, 10, 10))
	);
	if (cmd == NULL)
		goto fail;
	ec_keyval_set(ec_node_attrs(cmd), "help",
		"say hello to someone several times", NULL);
	ec_keyval_set(ec_node_attrs(ec_node_find(cmd, "john")),
		"help", "specific help for john", NULL);
	ec_keyval_set(ec_node_attrs(ec_node_find(cmd, "name")),
		"help", "the name of the person", NULL);
	ec_keyval_set(ec_node_attrs(ec_node_find(cmd, "int")),
		"help", "an integer (0-10)", NULL);
	if (ec_node_or_add(cmdlist, cmd) < 0)
		goto fail;


	cmd = EC_NODE_CMD(NULL, "good morning name [count]",
			EC_NODE_CMD("name", "bob|bobby|michael"),
			ec_node_int("count", 0, 10, 10));
	if (cmd == NULL)
		goto fail;
	ec_keyval_set(ec_node_attrs(cmd), "help",
		"say good morning to someone several times", NULL);
	ec_keyval_set(ec_node_attrs(ec_node_find(cmd, "name")), "help",
		"the person to greet", NULL);
	ec_keyval_set(ec_node_attrs(ec_node_find(cmd, "count")), "help",
		"how many times to greet", NULL);
	if (ec_node_or_add(cmdlist, cmd) < 0)
		goto fail;


	cmd = EC_NODE_CMD(NULL,
			"buy potatoes,carrots,pumpkins");
	if (cmd == NULL)
		goto fail;
	ec_keyval_set(ec_node_attrs(cmd), "help",
		"buy some vegetables", NULL);
	if (ec_node_or_add(cmdlist, cmd) < 0)
		goto fail;


	cmd = EC_NODE_CMD(NULL, "eat vegetables",
			ec_node_many("vegetables",
				EC_NODE_OR(NULL,
					ec_node_str(NULL, "potatoes"),
					ec_node_once(NULL,
						ec_node_str(NULL, "carrots")),
					ec_node_once(NULL,
						ec_node_str(NULL, "pumpkins"))),
			1, 0));
	if (cmd == NULL)
		goto fail;
	ec_keyval_set(ec_node_attrs(cmd), "help",
		"eat vegetables (take some more potatoes)", NULL);
	if (ec_node_or_add(cmdlist, cmd) < 0)
		goto fail;


	cmd = EC_NODE_SEQ(NULL,
		ec_node_str(NULL, "bye")
	);
	ec_keyval_set(ec_node_attrs(cmd), "help", "say bye", NULL);
	if (ec_node_or_add(cmdlist, cmd) < 0)
		goto fail;


	cmd = EC_NODE_SEQ(NULL,
		ec_node_str(NULL, "load"),
		ec_node("file", NULL)
	);
	ec_keyval_set(ec_node_attrs(cmd), "help", "load a file", NULL);
	if (ec_node_or_add(cmdlist, cmd) < 0)
		goto fail;


	commands = ec_node_sh_lex(NULL, cmdlist);
	if (commands == NULL)
		goto fail;

	return 0;

 fail:
	fprintf(stderr, "cannot initialize nodes\n");
	ec_node_free(cmdlist);
	return -1;
}

int main(void)
{
	struct ec_parsed *p;
	char *line;

	if (create_commands() < 0)
		return 1;

	rl_bind_key('?', show_help);
	rl_attempted_completion_function = my_attempted_completion;

	while (1) {
		line = readline("> ");
		if (line == NULL)
			break;

		p = ec_node_parse(commands, line);
		ec_parsed_dump(stdout, p);
		add_history(line);
		ec_parsed_free(p);
	}


	ec_node_free(commands);
	return 0;

}
