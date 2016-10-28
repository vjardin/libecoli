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

#include <ecoli_log.h>

static ec_log_t ec_log_fct = ec_log_default;
static void *ec_log_opaque;

int ec_log_default(unsigned int level, void *opaque, const char *str)
{
	(void)opaque;
	(void)level;

	return printf("%s", str);
}

int ec_log_register(ec_log_t usr_log, void *opaque)
{
	if (usr_log == NULL)
		return -1;

	ec_log_fct = usr_log;
	ec_log_opaque = opaque;

	return 0;
}

void ec_log_unregister(void)
{
	ec_log_fct = NULL;
}

int ec_vlog(unsigned int level, const char *format, va_list ap)
{
	char *s;
	int ret;

	if (ec_log_fct == NULL) {
		errno = ENODEV;
		return -1;
	}

	ret = vasprintf(&s, format, ap);
	if (ret < 0)
		return ret;

	ret = ec_log_fct(level, ec_log_opaque, s);
	free(s);

	return ret;
}

int ec_log(unsigned int level, const char *format, ...)
{
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = ec_vlog(level, format, ap);
	va_end(ap);

	return ret;
}
