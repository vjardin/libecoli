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

/**
 * Logging API
 *
 * This file provide logging helpers:
 * - logging functions, supporting printf-like format
 * - several debug level (similar to syslog)
 * - named log types
 * - redirection of log to a user functions (default logs nothing)
 */

#ifndef ECOLI_LOG_
#define ECOLI_LOG_

#include <stdarg.h>

#include <ecoli_assert.h>

enum ec_log_level {
	EC_LOG_EMERG   = 0,  /* system is unusable               */
	EC_LOG_ALERT   = 1,  /* action must be taken immediately */
	EC_LOG_CRIT    = 2,  /* critical conditions              */
	EC_LOG_ERR     = 3,  /* error conditions                 */
	EC_LOG_WARNING = 4,  /* warning conditions               */
	EC_LOG_NOTICE  = 5,  /* normal but significant condition */
	EC_LOG_INFO    = 6,  /* informational                    */
	EC_LOG_DEBUG   = 7,  /* debug-level messages             */
};

/**
 * Register a log type.
 *
 * This macro defines a function that will be called at startup (using
 * the "constructor" attribute). This function register the named type
 * passed as argument, and sets a static global variable
 * "ec_log_local_type". This variable is used as the default log type
 * for this file when using EC_LOG() or EC_VLOG().
 *
 * This macro can be present several times in a file. In this case, the
 * local log type is set to the last registered type.
 *
 * On error, the function aborts.
 *
 * @param name
 *   The name of the log to be registered.
 */
#define EC_LOG_TYPE_REGISTER(name)					\
	static int name##_log_type;					\
	static int ec_log_local_type;					\
	__attribute__((constructor, used))				\
	static void ec_log_register_##name(void)			\
	{								\
		ec_log_local_type = ec_log_type_register(#name);	\
		ec_assert_print(ec_log_local_type >= 0,			\
				"cannot register log type.\n");		\
		name##_log_type = ec_log_local_type;			\
	}

/**
 * User log function type.
 *
 * It is advised that a user-defined log function drops all messages
 * that are at least as critical as ec_log_level_get(), as done by the
 * default handler.
 *
 * @param type
 *   The log type identifier.
 * @param level
 *   The log level.
 * @param opaque
 *   The opaque pointer that was passed to ec_log_fct_register().
 * @param str
 *   The string to log.
 * @return
 *   0 on success, -1 on error (errno is set).
 */
typedef int (*ec_log_t)(int type, enum ec_log_level level, void *opaque,
			const char *str);

/**
 * Register a user log function.
 *
 * @param usr_log
 *   Function pointer that will be invoked for each log call.
 *   If the parameter is NULL, ec_log_default_cb() is used.
 * @param opaque
 *   Opaque pointer passed to the log function.
 * @return
 *   0 on success, -1 on error (errno is set).
 */
int ec_log_fct_register(ec_log_t usr_log, void *opaque);

/**
 * Register a named log type.
 *
 * Register a new log type, which is identified by its name. The
 * function returns a log identifier associated to the log name. If the
 * name is already registered, the function just returns its identifier.
 *
 * @param name
 *   The name of the log type.
 * @return
 *   The log type identifier on success (positive or zero), -1 on
 *   error (errno is set).
 */
int ec_log_type_register(const char *name);

/**
 * Return the log name associated to the log type identifier.
 *
 * @param type
 *   The log type identifier.
 * @return
 *   The name associated to the log type, or "unknown". It always return
 *   a valid string (never NULL).
 */
const char *ec_log_name(int type);

/**
 * Log a formatted string.
 *
 * @param type
 *   The log type identifier.
 * @param level
 *   The log level.
 * @param format
 *   The format string, followed by optional arguments.
 * @return
 *   0 on success, -1 on error (errno is set).
 */
int ec_log(int type, enum ec_log_level level, const char *format, ...)
	__attribute__((format(__printf__, 3, 4)));

/**
 * Log a formatted string.
 *
 * @param type
 *   The log type identifier.
 * @param level
 *   The log level.
 * @param format
 *   The format string.
 * @param ap
 *   The list of arguments.
 * @return
 *   0 on success, -1 on error (errno is set).
 */
int ec_vlog(int type, enum ec_log_level level, const char *format, va_list ap);

/**
 * Log a formatted string using the local log type.
 *
 * This macro requires that a log type is previously register with
 * EC_LOG_TYPE_REGISTER() since it uses the "ec_log_local_type"
 * variable.
 *
 * @param level
 *   The log level.
 * @param format
 *   The format string, followed by optional arguments.
 * @return
 *   0 on success, -1 on error (errno is set).
 */
#define EC_LOG(level, args...) ec_log(ec_log_local_type, level, args)

/**
 * Log a formatted string using the local log type.
 *
 * This macro requires that a log type is previously register with
 * EC_LOG_TYPE_REGISTER() since it uses the "ec_log_local_type"
 * variable.
 *
 * @param level
 *   The log level.
 * @param format
 *   The format string.
 * @param ap
 *   The list of arguments.
 * @return
 *   0 on success, -1 on error (errno is set).
 */
#define EC_VLOG(level, fmt, ap) ec_vlog(ec_log_local_type, level, fmt, ap)

/**
 * Default log handler.
 *
 * This is the default log function that is used by the library. By
 * default, it prints all logs whose level is WARNING or more critical.
 * This level can be changed with ec_log_level_set().
 *
 * @param type
 *   The log type identifier.
 * @param level
 *   The log level.
 * @param opaque
 *   Unused.
 * @param str
 *   The string to be logged.
 * @return
 *   0 on success, -1 on error (errno is set).
 */
int ec_log_default_cb(int type, enum ec_log_level level, void *opaque,
		const char *str);

/**
 * Set the global log level.
 *
 * This level is used by the default log handler, ec_log_default_cb().
 * All messages that are at least as critical as the default level are
 * displayed.
 *
 * It is advised
 *
 * @param level
 *   The log level to be set.
 * @return
 *   0 on success, -1 on error.
 */
int ec_log_level_set(enum ec_log_level level);

/**
 * Get the global log level.
 *
 * This level is used by the default log handler, ec_log_default_cb().
 * All messages that are at least as critical as the default level are
 * displayed.
 *
 * @param level
 *   The log level to be set.
 * @return
 *   0 on success, -1 on error.
 */
enum ec_log_level ec_log_level_get(void);

#endif
