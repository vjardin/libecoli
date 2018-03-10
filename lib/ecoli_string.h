/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#ifndef ECOLI_STRING_
#define ECOLI_STRING_

#include <stddef.h>

/* count the number of identical chars at the beginning of 2 strings */
size_t ec_strcmp_count(const char *s1, const char *s2);

/* return 1 if 's' starts with 'beginning' */
int ec_str_startswith(const char *s, const char *beginning);

/* like asprintf, but use libecoli allocator */
int ec_asprintf(char **buf, const char *fmt, ...);

/* like vasprintf, but use libecoli allocator */
int ec_vasprintf(char **buf, const char *fmt, va_list ap);

#endif
