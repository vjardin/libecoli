/*
 * Copyright 2018 6WIND S.A.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>

#include <histedit.h>

#include "string_utils.h"
#include "editline.h"

#define NC_CLI_HISTORY_SIZE 128

struct nc_cli_editline {
	EditLine *editline;
	History *history;
	HistEvent histev;
	bool break_received;
	nc_cli_editline_complete_t complete;
	FILE *null_out;
	bool interactive;
	bool incomplete_line;
	char *full_line;
	char *(*prompt_cb)(EditLine *);
};

struct nc_cli_editline *nc_cli_el;

static int check_quotes(const char *str)
{
	char quote = 0;
	size_t i = 0;

	while (str[i] != '\0') {
		if (quote == 0) {
			if (str[i] == '"' || str[i] == '\'') {
				quote = str[i];
			}
			i++;
			continue;
		} else {
			if (str[i] == quote) {
				i++;
				quote = 0;
			} else if (str[i] == '\\' && str[i+1] == quote) {
				i += 2;
			} else {
				i++;
			}
			continue;
		}
	}

	return quote;
}

static int
editline_break(EditLine *editline, int c)
{
	struct nc_cli_editline *el;
	void *ptr;

	(void)c;

	if (el_get(editline, EL_CLIENTDATA, &ptr))
		return CC_ERROR;

	el = ptr;
	el->break_received = true;
	nc_cli_printf(el, "\n");

	return CC_EOF;
}

static int
editline_suspend(EditLine *editline, int c)
{
	(void)editline;
	(void)c;

	kill(getpid(), SIGSTOP);

	return CC_NORM;
}

int
editline_get_screen_size(struct nc_cli_editline *el, size_t *rows, size_t *cols)
{
	int w, h;

	if (rows != NULL) {
		if (el_get(el->editline, EL_GETTC, "li", &h, (void *)0))
			return -1;
		*rows = h;
	}
	if (cols != NULL) {
		if (el_get(el->editline, EL_GETTC, "co", &w, (void *)0))
			return -1;
		*cols = w;
	}
	return 0;
}

FILE *
nc_cli_editline_get_file(struct nc_cli_editline *el, int num)
{
	FILE *f;

	if (el == NULL)
		return NULL;
	if (num > 2)
		return NULL;
	if (el_get(el->editline, EL_GETFP, num, &f))
		return NULL;

	return f;
}

/* match the prototype expected by qsort() */
static int
strcasecmp_cb(const void *p1, const void *p2)
{
	return strcasecmp(*(char * const *)p1, *(char * const *)p2);
}

/* Show the matches as a multi-columns list */
int
nc_cli_editline_print_cols(struct nc_cli_editline *el,
			char const * const *matches, size_t n)
{
	size_t max_strlen = 0, len, i, j, ncols;
	size_t width, height;
	const char *space;
	char **matches_copy = NULL;

	nc_cli_printf(nc_cli_el, "\n");
	if (n == 0)
		return 0;

	if (editline_get_screen_size(el, &height, &width) < 0)
		width = 80;

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
			nc_cli_printf(nc_cli_el, "%s%-*s", space,
				(int)max_strlen, matches[i+j]);
		}
		nc_cli_printf(nc_cli_el, "\n");
	}

	free(matches_copy);
	return 0;
}

static int
editline_complete(EditLine *editline, int c)
{
	enum nc_cli_editline_complete_status ret;
	const LineInfo *line_info;
	struct nc_cli_editline *el;
	int len;
	char *line;
	void *ptr;

	if (el_get(editline, EL_CLIENTDATA, &ptr))
		return CC_ERROR;

	el = ptr;

	if (el->complete == NULL)
		return CC_NORM;

	line_info = el_line(editline);
	if (line_info == NULL)
		return CC_ERROR;

	len = line_info->cursor - line_info->buffer;
	if (asprintf(&line, "%s%.*s", el->full_line ? : "", len,
			line_info->buffer) < 0)
		return CC_ERROR;

	if (c == '?' && check_quotes(line) != 0) {
		free(line);
		el_insertstr(editline, "?");
		return CC_REFRESH;
	}

	ret = el->complete(c, line);
	free(line);

	if (ret == ERROR)
		return CC_ERROR;
	else if (ret == REDISPLAY)
		return CC_REDISPLAY;
	else
		return CC_REFRESH;
}

static bool is_blank_string(const char *s)
{
	while (*s) {
		if (!isspace(*s))
			return false;
		s++;
	}
	return true;
}

static char *
multiline_prompt_cb(EditLine *e)
{
	(void)e;
	return strdup("... ");
}

const char *
nc_cli_editline_edit(struct nc_cli_editline *el)
{
	const char *line;
	int count;

	assert(el->editline != NULL);

	if (el->incomplete_line == false) {
		free(el->full_line);
		el->full_line = NULL;
	}

	el->break_received = false;

	if (el->incomplete_line)
		el_set(el->editline, EL_PROMPT, multiline_prompt_cb);
	else
		el_set(el->editline, EL_PROMPT, el->prompt_cb);

	line = el_gets(el->editline, &count);

	if (line == NULL && el->break_received) {
		free(el->full_line);
		el->full_line = NULL;
		el->incomplete_line = false;
		return ""; /* abort current line */
	}

	if (line == NULL || astrcat(&el->full_line, line) < 0) {
		free(el->full_line);
		el->full_line = NULL;
		el->incomplete_line = false;
		return NULL; /* error / eof */
	}

	if (check_quotes(el->full_line) != 0) {
		el->incomplete_line = true;
		return "";
	}

	el->incomplete_line = false;
	if (el->history != NULL && !is_blank_string(el->full_line))
		history(el->history, &el->histev,
			H_ENTER, el->full_line);

	return el->full_line;
}

int
nc_cli_editline_register_complete(struct nc_cli_editline *el,
				nc_cli_editline_complete_t complete)
{
	const char *name;

	if (el_set(el->editline, EL_ADDFN, "ed-complete",
			"Complete buffer",
			editline_complete))
		return -1;

	if (complete != NULL)
		name = "ed-complete";
	else
		name = "ed-unassigned";
	if (el_set(el->editline, EL_BIND, "^I", name, NULL))
		return -1;

	if (complete != NULL)
		name = "ed-complete";
	else
		name = "ed-insert";
	if (el_set(el->editline, EL_BIND, "?", name, NULL))
		return -1;

	el->complete = complete;

	return 0;
}

int
nc_cli_editline_insert_str(struct nc_cli_editline *el, const char *str)
{
	return el_insertstr(el->editline, str);
}

int nc_cli_printf(struct nc_cli_editline *el, const char *format, ...)
{
	FILE *out = nc_cli_editline_get_file(el, 1);
	va_list ap;
	int ret;

	if (out == NULL)
		out = stdout;

	va_start(ap, format);
	ret = vfprintf(out, format, ap);
	va_end(ap);

	return ret;
}

int nc_cli_eprintf(struct nc_cli_editline *el, const char *format, ...)
{
	FILE *out = nc_cli_editline_get_file(el, 2);
	va_list ap;
	int ret;

	if (out == NULL)
		out = stderr;

	va_start(ap, format);
	ret = vfprintf(out, format, ap);
	va_end(ap);

	return ret;
}

bool
nc_cli_editline_is_interactive(struct nc_cli_editline *el)
{
	return el->interactive;
}

bool
nc_cli_editline_is_running(struct nc_cli_editline *el)
{
	return el == nc_cli_el;
}

void
nc_cli_editline_start(struct nc_cli_editline *el)
{
	nc_cli_el = el;
}

void
nc_cli_editline_stop(struct nc_cli_editline *el)
{
	if (el == nc_cli_el)
		nc_cli_el = NULL;
}

int
nc_cli_editline_getc(struct nc_cli_editline *el)
{
	char c;

	if (el->interactive == false)
		return -1;

	if (el_getc(el->editline, &c) != 1)
		return -1;

	return c;
}

int
nc_cli_editline_resize(struct nc_cli_editline *el)
{
	el_resize(el->editline);
	return 0;
}

int nc_cli_editline_set_prompt_cb(struct nc_cli_editline *el,
				char *(*prompt_cb)(EditLine *el))
{
	el->prompt_cb = prompt_cb;
	return 0;
}

int
nc_cli_editline_mask_interrupts(bool do_mask)
{
	const char *setty = do_mask ? "-isig" : "+isig";

	if (nc_cli_el == NULL)
		return -1;

	if (nc_cli_el->interactive == false)
		return 0;

	if (el_set(nc_cli_el->editline, EL_SETTY, "-d", setty, NULL))
		return -1;

	return 0;
}

struct nc_cli_editline *
nc_cli_editline_init(FILE *f_in, FILE *f_out, FILE *f_err, bool interactive,
		char *(*prompt_cb)(EditLine *el))
{
	struct nc_cli_editline *el = NULL;

	el = calloc(1, sizeof(*el));
	if (el == NULL)
		goto fail;
	if (f_in == NULL) {
		errno = EINVAL;
		goto fail;
	}
	if (f_out == NULL || f_err == NULL) {
		el->null_out = fopen("/dev/null", "w");
		if (el->null_out == NULL)
			goto fail;
	}
	if (f_out == NULL)
		f_out = el->null_out;
	if (f_err == NULL)
		f_err = el->null_out;

	el->interactive = interactive;
	el->prompt_cb = prompt_cb;

	el->editline = el_init("nc-cli", f_in, f_out, f_err);
	if (el->editline == NULL)
		goto fail;

	if (el_set(el->editline, EL_SIGNAL, 1))
		goto fail;

	if (el_set(el->editline, EL_CLIENTDATA, el))
		goto fail;

	if (el_set(el->editline, EL_PROMPT, prompt_cb))
		goto fail;

	if (interactive == false)
		goto end;

	if (el_set(el->editline, EL_PREP_TERM, 0))
		goto fail;

	if (el_set(el->editline, EL_EDITOR, "emacs"))
		goto fail;

	if (el_set(el->editline, EL_SETTY, "-d", "-isig", NULL))
		goto fail;
	if (el_set(el->editline, EL_ADDFN, "ed-break",
			"Break and flush the buffer",
			editline_break))
		goto fail;
	if (el_set(el->editline, EL_BIND, "^C", "ed-break", NULL))
		goto fail;
	if (el_set(el->editline, EL_ADDFN, "ed-suspend",
			"Suspend the terminal",
			editline_suspend))
		goto fail;
	if (el_set(el->editline, EL_BIND, "^Z", "ed-suspend", NULL))
		goto fail;
	if (el_set(el->editline, EL_BIND, "^W", "ed-delete-prev-word", NULL))
		goto fail;

	el->history = history_init();
	if (!el->history)
		goto fail;
	if (history(el->history, &el->histev, H_SETSIZE,
			NC_CLI_HISTORY_SIZE) < 0)
		goto fail;
	if (history(el->history, &el->histev,
			H_SETUNIQUE, 1))
		goto fail;
	if (el_set(el->editline, EL_HIST, history,
			el->history))
		goto fail;

end:
	return el;

fail:
	if (el != NULL) {
		if (el->null_out != NULL)
			fclose(el->null_out);
		if (el->history != NULL)
			history_end(el->history);
		if (el->editline != NULL)
			el_end(el->editline);
		free(el);
	}
	return NULL;
}

void
nc_cli_editline_free(struct nc_cli_editline *el)
{
	if (el == NULL)
		return;
	nc_cli_editline_stop(el);
	if (el->null_out != NULL)
		fclose(el->null_out);
	if (el->history != NULL)
		history_end(el->history);
	el_end(el->editline);
	free(el->full_line);
	free(el);
}
