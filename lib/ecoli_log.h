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

#ifndef ECOLI_LOG_
#define ECOLI_LOG_

#define EC_LOG_EMERG    0  /* system is unusable               */
#define EC_LOG_ALERT    1  /* action must be taken immediately */
#define EC_LOG_CRIT     2  /* critical conditions              */
#define EC_LOG_ERR      3  /* error conditions                 */
#define EC_LOG_WARNING  4  /* warning conditions               */
#define EC_LOG_NOTICE   5  /* normal but significant condition */
#define EC_LOG_INFO     6  /* informational                    */
#define EC_LOG_DEBUG    7  /* debug-level messages             */

#include <stdarg.h>

/* return -1 on error, len(s) on success */
typedef int (*ec_log_t)(unsigned int level, void *opaque, const char *str);

int ec_log_register(ec_log_t usr_log, void *opaque);
void ec_log_unregister(void);

/* same api than printf */
int ec_log(unsigned int level, const char *format, ...);
int ec_vlog(unsigned int level, const char *format, va_list ap);

/* default log handler for the library, use printf */
int ec_log_default(unsigned int level, void *opaque, const char *str);

#endif
