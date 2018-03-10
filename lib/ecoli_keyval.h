/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016, Olivier MATZ <zer0@droids-corp.org>
 */

/**
 * Simple hash table API
 *
 * This file provides functions to store objects in hash tables, using strings
 * as keys.
 */

#ifndef ECOLI_KEYVAL_
#define ECOLI_KEYVAL_

#include <stdio.h>
#include <stdbool.h>

typedef void (*ec_keyval_elt_free_t)(void *);

struct ec_keyval;

/**
 * Create a hash table.
 *
 * @return
 *   The hash table, or NULL on error (errno is set).
 */
struct ec_keyval *ec_keyval(void);

/**
 * Get a value from the hash table.
 *
 * @param keyval
 *   The hash table.
 * @param key
 *   The key string.
 * @return
 *   The element if it is found, or NULL on error (errno is set).
 *   In case of success but the element is NULL, errno is set to 0.
 */
void *ec_keyval_get(const struct ec_keyval *keyval, const char *key);

/**
 * Check if the hash table contains this key.
 *
 * @param keyval
 *   The hash table.
 * @param key
 *   The key string.
 * @return
 *   true if it contains the key, else false.
 */
bool ec_keyval_has_key(const struct ec_keyval *keyval, const char *key);

/**
 * Delete an object from the hash table.
 *
 * @param keyval
 *   The hash table.
 * @param key
 *   The key string.
 * @return
 *   0 on success, or -1 on error (errno is set).
 */
int ec_keyval_del(struct ec_keyval *keyval, const char *key);

/**
 * Add/replace an object in the hash table.
 *
 * @param keyval
 *   The hash table.
 * @param key
 *   The key string.
 * @param val
 *   The pointer to be saved in the hash table.
 * @param free_cb
 *   An optional pointer to a destructor function called when an
 *   object is destroyed (ec_keyval_del() or ec_keyval_free()).
 * @return
 *   0 on success, or -1 on error (errno is set).
 *   On error, the passed value is freed (free_cb(val) is called).
 */
int ec_keyval_set(struct ec_keyval *keyval, const char *key, void *val,
	ec_keyval_elt_free_t free_cb);

/**
 * Free a hash table an all its objects.
 *
 * @param keyval
 *   The hash table.
 */
void ec_keyval_free(struct ec_keyval *keyval);

/**
 * Get the length of a hash table.
 *
 * @param keyval
 *   The hash table.
 * @return
 *   The length of the hash table.
 */
size_t ec_keyval_len(const struct ec_keyval *keyval);

/**
 * Duplicate a hash table
 *
 * A reference counter is shared between the clones of
 * hash tables so that the objects are freed only when
 * the last reference is destroyed.
 *
 * @param keyval
 *   The hash table.
 * @return
 *   The duplicated hash table, or NULL on error (errno is set).
 */
struct ec_keyval *ec_keyval_dup(const struct ec_keyval *keyval);

/**
 * Dump a hash table.
 *
 * @param out
 *   The stream where the dump is sent.
 * @param keyval
 *   The hash table.
 */
void ec_keyval_dump(FILE *out, const struct ec_keyval *keyval);

#endif
