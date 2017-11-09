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

#define _GNU_SOURCE /* for vasprintf */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_log.h>

static ec_log_t ec_log_fct = ec_log_default_cb;
static void *ec_log_opaque;

struct ec_log_type {
	char *name;
	enum ec_log_level level;
};

static struct ec_log_type *log_types;
static size_t log_types_len;
static enum ec_log_level global_level = EC_LOG_WARNING;

int ec_log_level_set(enum ec_log_level level)
{
	if (level < 0 || level > EC_LOG_DEBUG)
		return -1;
	global_level = level;

	return 0;
}

enum ec_log_level ec_log_level_get(void)
{
	return global_level;
}

int ec_log_default_cb(int type, enum ec_log_level level, void *opaque,
		const char *str)
{
	(void)opaque;

	if (level > ec_log_level_get())
		return 0;

	if (fprintf(stderr, "[%d] %-12s %s", level, ec_log_name(type), str) < 0)
		return -1;

	return 0;
}

int ec_log_fct_register(ec_log_t usr_log, void *opaque)
{
	errno = 0;

	if (usr_log == NULL) {
		ec_log_fct = ec_log_default_cb;
		ec_log_opaque = NULL;
	} else {
		ec_log_fct = usr_log;
		ec_log_opaque = opaque;
	}

	return 0;
}

static int
ec_log_lookup(const char *name)
{
	size_t i;

	for (i = 0; i < log_types_len; i++) {
		if (log_types[i].name == NULL)
			continue;
		if (strcmp(name, log_types[i].name) == 0)
			return i;
	}

	return -1;
}

int
ec_log_type_register(const char *name)
{
	struct ec_log_type *new_types;
	char *copy;
	int id;

	id = ec_log_lookup(name);
	if (id >= 0)
		return id;

	new_types = ec_realloc(log_types,
		sizeof(*new_types) * (log_types_len + 1));
	if (new_types == NULL)
		return -1; /* errno is set */
	log_types = new_types;

	copy = ec_strdup(name);
	if (copy == NULL)
		return -1; /* errno is set */

	id = log_types_len++;
	log_types[id].name = copy;
	log_types[id].level = EC_LOG_DEBUG;

	return id;
}

const char *
ec_log_name(int type)
{
	if (type < 0 || (unsigned int)type >= log_types_len)
		return "unknown";
	return log_types[type].name;
}

int ec_vlog(int type, enum ec_log_level level, const char *format, va_list ap)
{
	char *s;
	int ret;

	ret = vasprintf(&s, format, ap);
	if (ret < 0)
		return ret;

	ret = ec_log_fct(type, level, ec_log_opaque, s);
	free(s);

	return ret;
}

int ec_log(int type, enum ec_log_level level, const char *format, ...)
{
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = ec_vlog(type, level, format, ap);
	va_end(ap);

	return ret;
}
