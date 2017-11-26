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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

#include <ecoli_init.h>
#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_murmurhash.h>
#include <ecoli_keyval.h>

#define FACTOR 3

EC_LOG_TYPE_REGISTER(keyval);

static uint32_t ec_keyval_seed;

struct ec_keyval_elt {
	LIST_ENTRY(ec_keyval_elt) next;
	char *key;
	void *val;
	uint32_t hash;
	ec_keyval_elt_free_t free;
};

LIST_HEAD(ec_keyval_elt_list, ec_keyval_elt);

struct ec_keyval {
	size_t len;
	size_t table_size;
	struct ec_keyval_elt_list *table;
};

struct ec_keyval *ec_keyval(void)
{
	struct ec_keyval *keyval;

	keyval = ec_calloc(1, sizeof(*keyval));
	if (keyval == NULL)
		return NULL;

	return keyval;
}

static struct ec_keyval_elt *ec_keyval_lookup(const struct ec_keyval *keyval,
	const char *key)
{
	struct ec_keyval_elt *elt;
	uint32_t h, mask = keyval->table_size - 1;

	if (keyval == NULL || key == NULL) {
		errno = EINVAL;
		return NULL;
	}
	if (keyval->table_size == 0) {
		errno = ENOENT;
		return NULL;
	}

	h = ec_murmurhash3(key, strlen(key), ec_keyval_seed);
	LIST_FOREACH(elt, &keyval->table[h & mask], next) {
		if (strcmp(elt->key, key) == 0) {
			errno = 0;
			return elt;
		}
	}

	errno = ENOENT;
	return NULL;
}

static void ec_keyval_elt_free(struct ec_keyval_elt *elt)
{
	if (elt == NULL)
		return;

	ec_free(elt->key);
	if (elt->free != NULL)
		elt->free(elt->val);
	ec_free(elt);
}

bool ec_keyval_has_key(const struct ec_keyval *keyval, const char *key)
{
	return !!ec_keyval_lookup(keyval, key);
}

void *ec_keyval_get(const struct ec_keyval *keyval, const char *key)
{
	struct ec_keyval_elt *elt;

	elt = ec_keyval_lookup(keyval, key);
	if (elt == NULL)
		return NULL;

	return elt->val;
}

int ec_keyval_del(struct ec_keyval *keyval, const char *key)
{
	struct ec_keyval_elt *elt;

	elt = ec_keyval_lookup(keyval, key);
	if (elt == NULL)
		return -1;

	/* we could resize table here */

	LIST_REMOVE(elt, next);
	ec_keyval_elt_free(elt);
	keyval->len--;

	return 0;
}

static int ec_keyval_table_resize(struct ec_keyval *keyval, size_t new_size)
{
	struct ec_keyval_elt_list *new_table;
	struct ec_keyval_elt *elt;
	size_t i;

	if (new_size == 0 || (new_size & (new_size - 1))) {
		errno = EINVAL;
		return -1;
	}

	new_table = ec_calloc(new_size, sizeof(*keyval->table));
	if (new_table == NULL)
		return -1;

	for (i = 0; i < keyval->table_size; i++) {
		while (!LIST_EMPTY(&keyval->table[i])) {
			elt = LIST_FIRST(&keyval->table[i]);
			LIST_REMOVE(elt, next);
			LIST_INSERT_HEAD(&new_table[elt->hash & (new_size - 1)],
					elt, next);
		}
	}

	ec_free(keyval->table);
	keyval->table = new_table;
	keyval->table_size = new_size;

	return 0;
}

int ec_keyval_set(struct ec_keyval *keyval, const char *key, void *val,
	ec_keyval_elt_free_t free_cb)
{
	struct ec_keyval_elt *elt;
	uint32_t h, mask;
	size_t new_size;
	int ret;

	if (keyval == NULL || key == NULL) {
		errno = EINVAL;
		return -1;
	}

	elt = ec_keyval_lookup(keyval, key);
	if (elt != NULL) {
		if (elt->free != NULL)
			elt->free(elt->val);
		elt->val = val;
		elt->free = free_cb;
		return 0;
	}

	if (keyval->len >= keyval->table_size) {
		if (keyval->table_size != 0)
			new_size =  keyval->table_size << FACTOR;
		else
			new_size =  1 << FACTOR;
		ret = ec_keyval_table_resize(keyval, new_size);
		if (ret < 0)
			goto fail;
	}

	elt = ec_calloc(1, sizeof(*elt));
	if (elt == NULL)
		goto fail;
	elt->key = ec_strdup(key);
	if (elt->key == NULL)
		goto fail;
	elt->val = val;
	elt->free = free_cb;
	h = ec_murmurhash3(key, strlen(key), ec_keyval_seed);
	elt->hash = h;

	mask = keyval->table_size - 1;
	LIST_INSERT_HEAD(&keyval->table[h & mask], elt, next);
	keyval->len++;

	return 0;

fail:
	if (free_cb != NULL)
		free_cb(val);
	if (elt != NULL) {
		ec_free(elt->key);
		ec_free(elt);
	}

	return ret;
}

void ec_keyval_free(struct ec_keyval *keyval)
{
	struct ec_keyval_elt *elt;
	size_t i;

	if (keyval == NULL)
		return;

	for (i = 0; i < keyval->table_size; i++) {
		while (!LIST_EMPTY(&keyval->table[i])) {
			elt = LIST_FIRST(&keyval->table[i]);
			LIST_REMOVE(elt, next);
			ec_keyval_elt_free(elt);
		}
	}
	ec_free(keyval->table);
	ec_free(keyval);
}

size_t ec_keyval_len(const struct ec_keyval *keyval)
{
	return keyval->len;
}

void ec_keyval_dump(FILE *out, const struct ec_keyval *keyval)
{
	struct ec_keyval_elt *elt;
	size_t i;

	if (keyval == NULL) {
		fprintf(out, "empty keyval\n");
		return;
	}

	fprintf(out, "keyval:\n");
	for (i = 0; i < keyval->table_size; i++) {
		LIST_FOREACH(elt, &keyval->table[i], next) {
			fprintf(out, "  %s: %p\n",
				elt->key, elt->val);
		}
	}
}

static int ec_keyval_init_func(void)
{
	int fd;
	ssize_t ret;

	fd = open("/dev/urandom", 0);
	if (fd == -1) {
		fprintf(stderr, "failed to open /dev/urandom\n");
		return -1;
	}
	ret = read(fd, &ec_keyval_seed, sizeof(ec_keyval_seed));
	if (ret != sizeof(ec_keyval_seed)) {
		fprintf(stderr, "failed to read /dev/urandom\n");
		return -1;
	}
	close(fd);
	return 0;
}

static struct ec_init ec_keyval_init = {
	.init = ec_keyval_init_func,
	.priority = 50,
};

EC_INIT_REGISTER(ec_keyval_init);

/* LCOV_EXCL_START */
static int ec_keyval_testcase(void)
{
	struct ec_keyval *keyval;
	char *val;
	size_t i;
	int ret;

	keyval = ec_keyval();
	if (keyval == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create keyval\n");
		return -1;
	}

	EC_TEST_ASSERT(ec_keyval_len(keyval) == 0);
	ret = ec_keyval_set(keyval, "key1", "val1", NULL);
	EC_TEST_ASSERT_STR(ret == 0, "cannot set key");
	ret = ec_keyval_set(keyval, "key2", ec_strdup("val2"), ec_free_func);
	EC_TEST_ASSERT_STR(ret == 0, "cannot set key");
	EC_TEST_ASSERT(ec_keyval_len(keyval) == 2);

	val = ec_keyval_get(keyval, "key1");
	EC_TEST_ASSERT(val != NULL && !strcmp(val, "val1"));
	val = ec_keyval_get(keyval, "key2");
	EC_TEST_ASSERT(val != NULL && !strcmp(val, "val2"));
	val = ec_keyval_get(keyval, "key3");
	EC_TEST_ASSERT(val == NULL);

	ret = ec_keyval_set(keyval, "key1", "another_val1", NULL);
	EC_TEST_ASSERT_STR(ret == 0, "cannot set key");
	ret = ec_keyval_set(keyval, "key2", ec_strdup("another_val2"),
			ec_free_func);
	EC_TEST_ASSERT_STR(ret == 0, "cannot set key");
	EC_TEST_ASSERT(ec_keyval_len(keyval) == 2);

	val = ec_keyval_get(keyval, "key1");
	EC_TEST_ASSERT(val != NULL && !strcmp(val, "another_val1"));
	val = ec_keyval_get(keyval, "key2");
	EC_TEST_ASSERT(val != NULL && !strcmp(val, "another_val2"));
	EC_TEST_ASSERT(ec_keyval_has_key(keyval, "key1"));

	ret = ec_keyval_del(keyval, "key1");
	EC_TEST_ASSERT_STR(ret == 0, "cannot del key");
	EC_TEST_ASSERT(ec_keyval_len(keyval) == 1);

	ec_keyval_dump(stdout, NULL);
	ec_keyval_dump(stdout, keyval);

	for (i = 0; i < 100; i++) {
		char buf[8];
		snprintf(buf, sizeof(buf), "k%zd", i);
		ret = ec_keyval_set(keyval, buf, "val", NULL);
		EC_TEST_ASSERT_STR(ret == 0, "cannot set key");
	}

	/* einval */
	ret = ec_keyval_set(keyval, NULL, "val1", NULL);
	EC_TEST_ASSERT(ret == -1);
	val = ec_keyval_get(keyval, NULL);
	EC_TEST_ASSERT(val == NULL);

	ec_keyval_free(keyval);


	return 0;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_keyval_test = {
	.name = "keyval",
	.test = ec_keyval_testcase,
};

EC_TEST_REGISTER(ec_keyval_test);
