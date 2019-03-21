/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
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
#include <ecoli_dict.h>

#define FACTOR 3

EC_LOG_TYPE_REGISTER(dict);

static uint32_t ec_dict_seed;

struct ec_dict_elt {
	char *key;
	void *val;
	uint32_t hash;
	ec_dict_elt_free_t free;
	unsigned int refcount;
};

struct ec_dict_elt_ref {
	TAILQ_ENTRY(ec_dict_elt_ref) next;
	TAILQ_ENTRY(ec_dict_elt_ref) hnext;
	struct ec_dict_elt *elt;
};

TAILQ_HEAD(ec_dict_elt_ref_list, ec_dict_elt_ref);

struct ec_dict {
	size_t len;
	size_t table_size;
	struct ec_dict_elt_ref_list list;
	struct ec_dict_elt_ref_list *table;
};

struct ec_dict *ec_dict(void)
{
	struct ec_dict *dict;

	dict = ec_calloc(1, sizeof(*dict));
	if (dict == NULL)
		return NULL;
	TAILQ_INIT(&dict->list);

	return dict;
}

static struct ec_dict_elt_ref *
ec_dict_lookup(const struct ec_dict *dict, const char *key)
{
	struct ec_dict_elt_ref *ref;
	uint32_t h, mask = dict->table_size - 1;

	if (dict == NULL || key == NULL) {
		errno = EINVAL;
		return NULL;
	}
	if (dict->table_size == 0) {
		errno = ENOENT;
		return NULL;
	}

	h = ec_murmurhash3(key, strlen(key), ec_dict_seed);
	TAILQ_FOREACH(ref, &dict->table[h & mask], hnext) {
		if (strcmp(ref->elt->key, key) == 0)
			return ref;
	}

	errno = ENOENT;
	return NULL;
}

static void ec_dict_elt_ref_free(struct ec_dict_elt_ref *ref)
{
	struct ec_dict_elt *elt;

	if (ref == NULL)
		return;

	elt = ref->elt;
	if (elt != NULL && --elt->refcount == 0) {
		ec_free(elt->key);
		if (elt->free != NULL)
			elt->free(elt->val);
		ec_free(elt);
	}
	ec_free(ref);
}

bool ec_dict_has_key(const struct ec_dict *dict, const char *key)
{
	return !!ec_dict_lookup(dict, key);
}

void *ec_dict_get(const struct ec_dict *dict, const char *key)
{
	struct ec_dict_elt_ref *ref;

	ref = ec_dict_lookup(dict, key);
	if (ref == NULL)
		return NULL;

	return ref->elt->val;
}

int ec_dict_del(struct ec_dict *dict, const char *key)
{
	struct ec_dict_elt_ref *ref;
	size_t idx;

	ref = ec_dict_lookup(dict, key);
	if (ref == NULL)
		return -1;

	/* we could resize table here */

	TAILQ_REMOVE(&dict->list, ref, next);
	idx = ref->elt->hash & (dict->table_size - 1);
	TAILQ_REMOVE(&dict->table[idx], ref, hnext);
	ec_dict_elt_ref_free(ref);
	dict->len--;

	return 0;
}

static int ec_dict_table_resize(struct ec_dict *dict, size_t new_size)
{
	struct ec_dict_elt_ref_list *new_table;
	struct ec_dict_elt_ref *ref;
	size_t i;

	if (new_size == 0 || (new_size & (new_size - 1))) {
		errno = EINVAL;
		return -1;
	}

	new_table = ec_calloc(new_size, sizeof(*dict->table));
	if (new_table == NULL)
		return -1;
	for (i = 0; i < new_size; i++)
		TAILQ_INIT(&new_table[i]);

	TAILQ_FOREACH(ref, &dict->list, next) {
		i = ref->elt->hash & (dict->table_size - 1);
		TAILQ_REMOVE(&dict->table[i], ref, hnext);
		i = ref->elt->hash & (new_size - 1);
		TAILQ_INSERT_TAIL(&new_table[i], ref, hnext);
	}

	ec_free(dict->table);
	dict->table = new_table;
	dict->table_size = new_size;

	return 0;
}

static int
__ec_dict_set(struct ec_dict *dict, struct ec_dict_elt_ref *ref)
{
	size_t new_size;
	uint32_t mask;
	int ret;

	/* remove previous entry if any */
	ec_dict_del(dict, ref->elt->key);

	if (dict->len >= dict->table_size) {
		if (dict->table_size != 0)
			new_size =  dict->table_size << FACTOR;
		else
			new_size =  1 << FACTOR;
		ret = ec_dict_table_resize(dict, new_size);
		if (ret < 0)
			return ret;
	}

	mask = dict->table_size - 1;
	TAILQ_INSERT_TAIL(&dict->table[ref->elt->hash & mask], ref, hnext);
	TAILQ_INSERT_TAIL(&dict->list, ref, next);
	dict->len++;

	return 0;
}

int ec_dict_set(struct ec_dict *dict, const char *key, void *val,
	ec_dict_elt_free_t free_cb)
{
	struct ec_dict_elt *elt = NULL;
	struct ec_dict_elt_ref *ref = NULL;
	uint32_t h;

	if (dict == NULL || key == NULL) {
		errno = EINVAL;
		return -1;
	}

	ref = ec_calloc(1, sizeof(*ref));
	if (ref == NULL)
		goto fail;

	elt = ec_calloc(1, sizeof(*elt));
	if (elt == NULL)
		goto fail;

	ref->elt = elt;
	elt->refcount = 1;
	elt->val = val;
	val = NULL;
	elt->free = free_cb;
	elt->key = ec_strdup(key);
	if (elt->key == NULL)
		goto fail;
	h = ec_murmurhash3(key, strlen(key), ec_dict_seed);
	elt->hash = h;

	if (__ec_dict_set(dict, ref) < 0)
		goto fail;

	return 0;

fail:
	if (free_cb != NULL && val != NULL)
		free_cb(val);
	ec_dict_elt_ref_free(ref);
	return -1;
}

void ec_dict_free(struct ec_dict *dict)
{
	struct ec_dict_elt_ref *ref;
	size_t idx;

	if (dict == NULL)
		return;

	while (!TAILQ_EMPTY(&dict->list)) {
		ref = TAILQ_FIRST(&dict->list);
		TAILQ_REMOVE(&dict->list, ref, next);
		idx = ref->elt->hash & (dict->table_size - 1);
		TAILQ_REMOVE(&dict->table[idx], ref, hnext);
		ec_dict_elt_ref_free(ref);
	}
	ec_free(dict->table);
	ec_free(dict);
}

size_t ec_dict_len(const struct ec_dict *dict)
{
	return dict->len;
}

struct ec_dict_elt_ref *
ec_dict_iter(const struct ec_dict *dict)
{
	if (dict == NULL)
		return NULL;

	return TAILQ_FIRST(&dict->list);
}

struct ec_dict_elt_ref *
ec_dict_iter_next(struct ec_dict_elt_ref *iter)
{
	if (iter == NULL)
		return NULL;

	return TAILQ_NEXT(iter, next);
}

const char *
ec_dict_iter_get_key(const struct ec_dict_elt_ref *iter)
{
	if (iter == NULL)
		return NULL;

	return iter->elt->key;
}

void *
ec_dict_iter_get_val(const struct ec_dict_elt_ref *iter)
{
	if (iter == NULL)
		return NULL;

	return iter->elt->val;
}

void ec_dict_dump(FILE *out, const struct ec_dict *dict)
{
	struct ec_dict_elt_ref *iter;

	if (dict == NULL) {
		fprintf(out, "empty dict\n");
		return;
	}

	fprintf(out, "dict:\n");
	for (iter = ec_dict_iter(dict);
	     iter != NULL;
	     iter = ec_dict_iter_next(iter)) {
		fprintf(out, "  %s: %p\n",
			ec_dict_iter_get_key(iter),
			ec_dict_iter_get_val(iter));
	}
}

struct ec_dict *ec_dict_dup(const struct ec_dict *dict)
{
	struct ec_dict *dup = NULL;
	struct ec_dict_elt_ref *ref, *dup_ref = NULL;

	dup = ec_dict();
	if (dup == NULL)
		return NULL;

	TAILQ_FOREACH(ref, &dict->list, next) {
		dup_ref = ec_calloc(1, sizeof(*ref));
		if (dup_ref == NULL)
			goto fail;
		dup_ref->elt = ref->elt;
		ref->elt->refcount++;

		if (__ec_dict_set(dup, dup_ref) < 0)
			goto fail;
	}

	return dup;

fail:
	ec_dict_elt_ref_free(dup_ref);
	ec_dict_free(dup);
	return NULL;
}

static int ec_dict_init_func(void)
{
	int fd;
	ssize_t ret;

	return 0;//XXX for test reproduceability

	fd = open("/dev/urandom", 0);
	if (fd == -1) {
		fprintf(stderr, "failed to open /dev/urandom\n");
		return -1;
	}
	ret = read(fd, &ec_dict_seed, sizeof(ec_dict_seed));
	if (ret != sizeof(ec_dict_seed)) {
		fprintf(stderr, "failed to read /dev/urandom\n");
		return -1;
	}
	close(fd);
	return 0;
}

static struct ec_init ec_dict_init = {
	.init = ec_dict_init_func,
	.priority = 50,
};

EC_INIT_REGISTER(ec_dict_init);

/* LCOV_EXCL_START */
static int ec_dict_testcase(void)
{
	struct ec_dict *dict, *dup;
	struct ec_dict_elt_ref *iter;
	char *val;
	size_t i, count;
	int ret, testres = 0;
	FILE *f = NULL;
	char *buf = NULL;
	size_t buflen = 0;

	dict = ec_dict();
	if (dict == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create dict\n");
		return -1;
	}

	count = 0;
	for (iter = ec_dict_iter(dict);
	     iter != NULL;
	     iter = ec_dict_iter_next(iter)) {
		count++;
	}
	testres |= EC_TEST_CHECK(count == 0, "invalid count in iterator");

	testres |= EC_TEST_CHECK(ec_dict_len(dict) == 0, "bad dict len");
	ret = ec_dict_set(dict, "key1", "val1", NULL);
	testres |= EC_TEST_CHECK(ret == 0, "cannot set key");
	ret = ec_dict_set(dict, "key2", ec_strdup("val2"), ec_free_func);
	testres |= EC_TEST_CHECK(ret == 0, "cannot set key");
	testres |= EC_TEST_CHECK(ec_dict_len(dict) == 2, "bad dict len");

	val = ec_dict_get(dict, "key1");
	testres |= EC_TEST_CHECK(val != NULL && !strcmp(val, "val1"),
				"invalid dict value");
	val = ec_dict_get(dict, "key2");
	testres |= EC_TEST_CHECK(val != NULL && !strcmp(val, "val2"),
				"invalid dict value");
	val = ec_dict_get(dict, "key3");
	testres |= EC_TEST_CHECK(val == NULL, "key3 should be NULL");

	ret = ec_dict_set(dict, "key1", "another_val1", NULL);
	testres |= EC_TEST_CHECK(ret == 0, "cannot set key");
	ret = ec_dict_set(dict, "key2", ec_strdup("another_val2"),
			ec_free_func);
	testres |= EC_TEST_CHECK(ret == 0, "cannot set key");
	testres |= EC_TEST_CHECK(ec_dict_len(dict) == 2,
				"bad dict len");

	val = ec_dict_get(dict, "key1");
	testres |= EC_TEST_CHECK(val != NULL && !strcmp(val, "another_val1"),
		"invalid dict value");
	val = ec_dict_get(dict, "key2");
	testres |= EC_TEST_CHECK(val != NULL && !strcmp(val, "another_val2"),
		"invalid dict value");
	testres |= EC_TEST_CHECK(ec_dict_has_key(dict, "key1"),
		"key1 should be in dict");

	f = open_memstream(&buf, &buflen);
	if (f == NULL)
		goto fail;
	ec_dict_dump(f, NULL);
	fclose(f);
	f = NULL;
	free(buf);
	buf = NULL;

	f = open_memstream(&buf, &buflen);
	if (f == NULL)
		goto fail;
	ec_dict_dump(f, dict);
	fclose(f);
	f = NULL;
	free(buf);
	buf = NULL;

	ret = ec_dict_del(dict, "key1");
	testres |= EC_TEST_CHECK(ret == 0, "cannot del key1");
	testres |= EC_TEST_CHECK(ec_dict_len(dict) == 1,
		"invalid dict len");
	ret = ec_dict_del(dict, "key2");
	testres |= EC_TEST_CHECK(ret == 0, "cannot del key2");
	testres |= EC_TEST_CHECK(ec_dict_len(dict) == 0,
		"invalid dict len");

	for (i = 0; i < 100; i++) {
		char key[8];
		snprintf(key, sizeof(key), "k%zd", i);
		ret = ec_dict_set(dict, key, "val", NULL);
		testres |= EC_TEST_CHECK(ret == 0, "cannot set key");
	}
	dup = ec_dict_dup(dict);
	testres |= EC_TEST_CHECK(dup != NULL, "cannot duplicate dict");
	if (dup != NULL) {
		for (i = 0; i < 100; i++) {
			char key[8];
			snprintf(key, sizeof(key), "k%zd", i);
			val = ec_dict_get(dup, key);
			testres |= EC_TEST_CHECK(
				val != NULL && !strcmp(val, "val"),
				"invalid dict value");
		}
		ec_dict_free(dup);
		dup = NULL;
	}

	count = 0;
	for (iter = ec_dict_iter(dict);
	     iter != NULL;
	     iter = ec_dict_iter_next(iter)) {
		count++;
	}
	testres |= EC_TEST_CHECK(count == 100, "invalid count in iterator");

	/* einval */
	ret = ec_dict_set(dict, NULL, "val1", NULL);
	testres |= EC_TEST_CHECK(ret == -1, "should not be able to set key");
	val = ec_dict_get(dict, NULL);
	testres |= EC_TEST_CHECK(val == NULL, "get(NULL) should no success");

	ec_dict_free(dict);

	return testres;

fail:
	ec_dict_free(dict);
	if (f)
		fclose(f);
	free(buf);
	return -1;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_dict_test = {
	.name = "dict",
	.test = ec_dict_testcase,
};

EC_TEST_REGISTER(ec_dict_test);
