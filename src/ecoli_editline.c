/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018, Olivier MATZ <zer0@droids-corp.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <histedit.h>

#include <ecoli_utils.h>
#include <ecoli_malloc.h>
#include <ecoli_string.h>
#include <ecoli_editline.h>
#include <ecoli_keyval.h>
#include <ecoli_node.h>
#include <ecoli_parse.h>
#include <ecoli_complete.h>

struct ec_editline {
	EditLine *el;
	History *history;
	HistEvent histev;
	const struct ec_node *node;
	char *prompt;
};

/* used by qsort below */
static int
strcasecmp_cb(const void *p1, const void *p2)
{
	return strcasecmp(*(char * const *)p1, *(char * const *)p2);
}

/* Show the matches as a multi-columns list */
int
ec_editline_print_cols(struct ec_editline *editline,
		char const * const *matches, size_t n)
{
	size_t max_strlen = 0, len, i, j, ncols;
	int width, height;
	const char *space;
	char **matches_copy = NULL;
	FILE *f;

	if (el_get(editline->el, EL_GETFP, 1, &f))
		return -1;

	fprintf(f, "\n");
	if (n == 0)
		return 0;

	if (el_get(editline->el, EL_GETTC, "li", &height, (void *)0))
		return -1;
	if (el_get(editline->el, EL_GETTC, "co", &width, (void *)0))
		return -1;

	/* duplicate the matches table, and sort it */
	matches_copy = calloc(n, sizeof(const char *));
	if (matches_copy == NULL)
		return -1;
	memcpy(matches_copy, matches, sizeof(const char *) * n);
	qsort(matches_copy, n, sizeof(char *), strcasecmp_cb);

	/* get max string length */
	for (i = 0; i < n; i++) {
		len = strlen(matches_copy[i]);
		if (len > max_strlen)
			max_strlen = len;
	}

	/* write the columns */
	ncols = width / (max_strlen + 4);
	if (ncols == 0)
		ncols = 1;
	for (i = 0; i < n; i+= ncols) {
		for (j = 0; j < ncols; j++) {
			if (i + j >= n)
				break;
			if (j == 0)
				space = "";
			else
				space = "    ";
			fprintf(f, "%s%-*s", space,
				(int)max_strlen, matches[i+j]);
		}
		fprintf(f, "\n");
	}

	free(matches_copy);
	return 0;
}

/* Show the helps on editline output */
int
ec_editline_print_helps(struct ec_editline *editline,
			const struct ec_editline_help *helps, size_t len)
{
	size_t i;
	FILE *out;

	if (el_get(editline->el, EL_GETFP, 1, &out))
		return -1;

	for (i = 0; i < len; i++) {
		if (fprintf(out, "%-20s %s\n",
				helps[i].desc, helps[i].help) < 0)
			return -1;
	}

	return 0;
}

void
ec_editline_free_helps(struct ec_editline_help *helps, size_t len)
{
	size_t i;

	if (helps == NULL)
		return;
	for (i = 0; i < len; i++) {
		ec_free(helps[i].desc);
		ec_free(helps[i].help);
	}
	ec_free(helps);
}

int
ec_editline_set_prompt(struct ec_editline *editline, const char *prompt)
{
	char *copy = NULL;

	if (prompt != NULL) {
		ec_strdup(prompt);
		if (copy == NULL)
			return -1;
	}

	ec_free(editline->prompt);
	editline->prompt = copy;

	return 0;
}

static char *
prompt_cb(EditLine *el)
{
	struct ec_editline *editline;
	void *clientdata;

	if (el_get(el, EL_CLIENTDATA, &clientdata))
		return "> ";
	editline = clientdata;

	if (editline == NULL)
		return "> ";

	return editline->prompt;
}

struct ec_editline *
ec_editline(const char *name, FILE *f_in, FILE *f_out, FILE *f_err,
	unsigned int flags)
{
	struct ec_editline *editline = NULL;
	EditLine *el;

	if (f_in == NULL || f_out == NULL || f_err == NULL) {
		errno = EINVAL;
		goto fail;
	}

	editline = ec_calloc(1, sizeof(*editline));
	if (editline == NULL)
		goto fail;

	el = el_init(name, f_in, f_out, f_err);
	if (el == NULL)
		goto fail;
	editline->el = el;

	/* save editline pointer as user data */
	if (el_set(el, EL_CLIENTDATA, editline))
		goto fail;

	/* install default editline signals */
	if (el_set(el, EL_SIGNAL, 1))
		goto fail;

	if (el_set(el, EL_PREP_TERM, 0))
		goto fail;

	/* use emacs bindings */
	if (el_set(el, EL_EDITOR, "emacs"))
		goto fail;
	if (el_set(el, EL_BIND, "^W", "ed-delete-prev-word", NULL))
		goto fail;

	/* ask terminal to not send signals */
	if (flags & EC_EDITLINE_DISABLE_SIGNALS) {
		if (el_set(el, EL_SETTY, "-d", "-isig", NULL))
			goto fail;
	}

	/* set prompt */
	editline->prompt = ec_strdup("> ");
	if (editline->prompt == NULL)
		goto fail;
	if (el_set(el, EL_PROMPT, prompt_cb))
		goto fail;

	/* set up history */
	if ((flags & EC_EDITLINE_DISABLE_HISTORY) == 0) {
		if (ec_editline_set_history(
				editline, EC_EDITLINE_HISTORY_SIZE) < 0)
			goto fail;
	}

	/* register completion callback */
	if ((flags & EC_EDITLINE_DISABLE_COMPLETION) == 0) {
		if (el_set(el, EL_ADDFN, "ed-complete", "Complete buffer",
				ec_editline_complete))
			goto fail;
		if (el_set(el, EL_BIND, "^I", "ed-complete", NULL))
			goto fail;
		if (el_set(el, EL_BIND, "?", "ed-complete", NULL))
			goto fail;
	}

	return editline;

fail:
	ec_editline_free(editline);
	return NULL;
}

void ec_editline_free(struct ec_editline *editline)
{
	if (editline == NULL)
		return;
	if (editline->el != NULL)
		el_end(editline->el);
	if (editline->history != NULL)
		history_end(editline->history);
	ec_free(editline->prompt);
	ec_free(editline);
}

EditLine *ec_editline_get_el(struct ec_editline *editline)
{
	return editline->el;
}

const struct ec_node *
ec_editline_get_node(struct ec_editline *editline)
{
	return editline->node;
}

void
ec_editline_set_node(struct ec_editline *editline, const struct ec_node *node)
{
	editline->node = node;
}

int ec_editline_set_history(struct ec_editline *editline,
	size_t hist_size)
{
	EditLine *el = editline->el;

	if (editline->history != NULL)
		history_end(editline->history);

	if (hist_size == 0)
		return 0;

	editline->history = history_init();
	if (editline->history == NULL)
		goto fail;
	if (history(editline->history, &editline->histev, H_SETSIZE,
			hist_size) < 0)
		goto fail;
	if (history(editline->history, &editline->histev, H_SETUNIQUE, 1))
		goto fail;
	if (el_set(el, EL_HIST, history, editline->history))
		goto fail;

	return 0;

fail:
	//XXX errno
	if (editline->history != NULL) {
		history_end(editline->history);
		editline->history = NULL;
	}
	return -1;
}

void ec_editline_free_completions(char **matches, size_t len)
{
	size_t i;

	// XXX use ec_malloc/ec_free() instead for consistency
	if (matches == NULL)
		return;
	for (i = 0; i < len; i++)
		free(matches[i]);
	free(matches);
}

ssize_t
ec_editline_get_completions(const struct ec_comp *cmpl, char ***matches_out)
{
	const struct ec_comp_item *item;
	struct ec_comp_iter *iter = NULL;
	char **matches = NULL;
	size_t count = 0;

	iter = ec_comp_iter(cmpl, EC_COMP_FULL | EC_COMP_PARTIAL);
	if (iter == NULL)
		goto fail;

	while ((item = ec_comp_iter_next(iter)) != NULL) {
		char **tmp;

		tmp = realloc(matches, (count + 1) * sizeof(char *));
		if (tmp == NULL)
			goto fail;
		matches = tmp;
		matches[count] = strdup(ec_comp_item_get_display(item));
		if (matches[count] == NULL)
			goto fail;
		count++;
	}

	*matches_out = matches;
	return count;

fail:
	ec_editline_free_completions(matches, count);
	*matches_out = NULL;
	ec_comp_iter_free(iter);
	return -1;
}

char *
ec_editline_append_chars(const struct ec_comp *cmpl)
{
	const struct ec_comp_item *item;
	struct ec_comp_iter *iter = NULL;
	const char *append;
	char *ret = NULL;
	size_t n;

	iter = ec_comp_iter(cmpl, EC_COMP_FULL | EC_COMP_PARTIAL);
	if (iter == NULL)
		goto fail;

	while ((item = ec_comp_iter_next(iter)) != NULL) {
		append = ec_comp_item_get_completion(item);
		if (ret == NULL) {
			ret = ec_strdup(append);
			if (ret == NULL)
				goto fail;
		} else {
			n = ec_strcmp_count(ret, append);
			ret[n] = '\0';
		}
	}
	ec_comp_iter_free(iter);

	return ret;

fail:
	ec_comp_iter_free(iter);
	ec_free(ret);

	return NULL;
}

/* this function builds the help string */
static int get_node_help(const struct ec_comp_item *item,
			struct ec_editline_help *help)
{
	const struct ec_comp_group *grp;
	const struct ec_parse *state;
	const struct ec_node *node;
	const char *node_help = NULL;
	const char *node_desc = NULL;

	help->desc = NULL;
	help->help = NULL;

	grp = ec_comp_item_get_grp(item);

	for (state = grp->state; state != NULL;
	     state = ec_parse_get_parent(state)) {
		node = ec_parse_get_node(state);
		if (node_help == NULL)
			node_help = ec_keyval_get(ec_node_attrs(node), "help");
		if (node_desc == NULL)
			node_desc = ec_node_desc(node);
	}

	if (node_help == NULL)
		node_help = "";
	if (node_desc == NULL)
		goto fail;

	help->desc = ec_strdup(node_desc);
	if (help->desc == NULL)
		goto fail;

	help->help = ec_strdup(node_help);
	if (help->help == NULL)
		goto fail;

	return 0;

fail:
	ec_free(help->desc);
	ec_free(help->help);
	return -1;
}

ssize_t
ec_editline_get_helps(const struct ec_editline *editline, const char *line,
	const char *full_line, struct ec_editline_help **helps_out)
{
	struct ec_comp_iter *iter = NULL;
	const struct ec_comp_group *grp, *prev_grp = NULL;
	const struct ec_comp_item *item;
	struct ec_comp *cmpl = NULL;
	struct ec_parse *parse = NULL;
	unsigned int count = 0;
	struct ec_editline_help *helps = NULL;

	*helps_out = NULL;

	/* check if the current line matches */
	parse = ec_node_parse(editline->node, full_line);
	if (ec_parse_matches(parse))
		count = 1;
	ec_parse_free(parse);
	parse = NULL;

	/* complete at current cursor position */
	cmpl = ec_node_complete(editline->node, line);
	if (cmpl == NULL) //XXX log error
		goto fail;

	/* let's display one contextual help per node */
	iter = ec_comp_iter(cmpl,
		EC_COMP_UNKNOWN | EC_COMP_FULL | EC_COMP_PARTIAL);
	if (iter == NULL)
		goto fail;

	helps = ec_calloc(1, sizeof(*helps));
	if (helps == NULL)
		goto fail;
	if (count == 1) {
		helps[0].desc = ec_strdup("<return>");
		if (helps[0].desc == NULL)
			goto fail;
		helps[0].help = ec_strdup("Validate command.");
		if (helps[0].help == NULL)
			goto fail;
	}

	while ((item = ec_comp_iter_next(iter)) != NULL) {
		struct ec_editline_help *tmp = NULL;

		/* keep one help per group, skip other items  */
		grp = ec_comp_item_get_grp(item);
		if (grp == prev_grp)
			continue;

		prev_grp = grp;

		tmp = ec_realloc(helps, (count + 1) * sizeof(*helps));
		if (tmp == NULL)
			goto fail;
		helps = tmp;
		if (get_node_help(item, &helps[count]) < 0)
			goto fail;
		count++;
	}

	ec_comp_iter_free(iter);
	ec_comp_free(cmpl);
	*helps_out = helps;

	return count;

fail:
	ec_comp_iter_free(iter);
	ec_parse_free(parse);
	ec_comp_free(cmpl);
	if (helps != NULL) {
		while (count--) {
			ec_free(helps[count].desc);
			ec_free(helps[count].help);
		}
		ec_free(helps);
	}

	return -1;
}

int
ec_editline_complete(EditLine *el, int c)
{
	struct ec_editline *editline;
	const LineInfo *line_info;
	int ret = CC_REFRESH;
	struct ec_comp *cmpl = NULL;
	char *append = NULL;
	char *line = NULL;
	void *clientdata;
	FILE *out, *err;
	int len;

	if (el_get(el, EL_GETFP, 1, &out))
		return -1;
	if (el_get(el, EL_GETFP, 1, &err))
		return -1;

	(void)c;

	if (el_get(el, EL_CLIENTDATA, &clientdata)) {
		fprintf(err, "completion failure: no client data\n");
		goto fail;
	}
	editline = clientdata;
	(void)editline;

	line_info = el_line(el);
	if (line_info == NULL) {
		fprintf(err, "completion failure: no line info\n");
		goto fail;
	}

	len = line_info->cursor - line_info->buffer;
	if (ec_asprintf(&line, "%.*s", len, line_info->buffer) < 0) {
		fprintf(err, "completion failure: no memory\n");
		goto fail;
	}

	if (editline->node == NULL) {
		fprintf(err, "completion failure: no ec_node\n");
		goto fail;
	}

	cmpl = ec_node_complete(editline->node, line);
	if (cmpl == NULL)
		goto fail;

	append = ec_editline_append_chars(cmpl);

	if (c == '?') {
		struct ec_editline_help *helps = NULL;
		ssize_t count = 0;

		count = ec_editline_get_helps(editline, line, line_info->buffer,
				&helps);

		fprintf(out, "\n");
		if (ec_editline_print_helps(editline, helps, count) < 0) {
			fprintf(err, "completion failure: cannot show help\n");
			ec_editline_free_helps(helps, count);
			goto fail;
		}

		ec_editline_free_helps(helps, count);
		ret = CC_REDISPLAY;
	} else if (append == NULL || strcmp(append, "") == 0) {
		char **matches = NULL;
		ssize_t count = 0;

		count = ec_editline_get_completions(cmpl, &matches);
		if (count < 0) {
			fprintf(err, "completion failure: cannot get completions\n");
			goto fail;
		}

		if (ec_editline_print_cols(
				editline,
				EC_CAST(matches, char **,
					char const * const *),
				count) < 0) {
			fprintf(err, "completion failure: cannot print\n");
			ec_editline_free_completions(matches, count);
			goto fail;
		}

		ec_editline_free_completions(matches, count);
		ret = CC_REDISPLAY;
	} else {
		if (el_insertstr(el, append) < 0) {
			fprintf(err, "completion failure: cannot insert\n");
			goto fail;
		}
		if (ec_comp_count(cmpl, EC_COMP_FULL) +
			ec_comp_count(cmpl, EC_COMP_PARTIAL) == 1) {
			if (el_insertstr(el, " ") < 0) {
				fprintf(err, "completion failure: cannot insert space\n");
				goto fail;
			}
		}
	}

	ec_comp_free(cmpl);
	ec_free(line);
	ec_free(append);

	return ret;

fail:
	ec_comp_free(cmpl);
	ec_free(line);
	ec_free(append);

	return CC_ERROR;
}

char *
ec_editline_gets(struct ec_editline *editline)
{
	EditLine *el = editline->el;
	char *line_copy = NULL;
	const char *line;
	int count;

	line = el_gets(el, &count);
	if (line == NULL)
		return NULL;

	line_copy = ec_strdup(line);
	if (line_copy == NULL)
		goto fail;

	line_copy[strlen(line_copy) - 1] = '\0'; //XXX needed because of sh_lex bug?

	if (editline->history != NULL && !ec_str_is_space(line_copy)) {
		history(editline->history, &editline->histev,
			H_ENTER, line_copy);
	}

	return line_copy;

fail:
	ec_free(line_copy);
	return NULL;
}

struct ec_parse *
ec_editline_parse(struct ec_editline *editline, const struct ec_node *node)
{
	char *line = NULL;
	struct ec_parse *parse = NULL;

	/* XXX add sh_lex automatically? This node is required, parse and
	 * complete are based on it. */

	ec_editline_set_node(editline, node);

	line = ec_editline_gets(editline);
	if (line == NULL)
		goto fail;

	parse = ec_node_parse(node, line);
	if (parse == NULL)
		goto fail;

	ec_free(line);
	return parse;

fail:
	ec_free(line);
	ec_parse_free(parse);

	return NULL;
}
