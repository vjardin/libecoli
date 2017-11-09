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
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_keyval.h>

EC_LOG_TYPE_REGISTER(keyval);

struct ec_keyval_elt {
	char *key;
	void *val;
	ec_keyval_elt_free_t free;
};

struct ec_keyval {
	size_t len;
	struct ec_keyval_elt *vec;
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
	size_t i;

	if (keyval == NULL || keyval->vec == NULL)
		return NULL;

	for (i = 0; i < ec_keyval_len(keyval); i++) {
		elt = &keyval->vec[i];
		if (strcmp(elt->key, key) == 0)
			return elt;
	}

	return NULL;
}

static void ec_keyval_elt_free(struct ec_keyval_elt *elt)
{
	if (elt == NULL)
		return;

	ec_free(elt->key);
	if (elt->free != NULL)
		elt->free(elt->val);
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
	struct ec_keyval_elt *last = &keyval->vec[keyval->len - 1];

	elt = ec_keyval_lookup(keyval, key);
	if (elt == NULL)
		return -ENOENT;

	ec_keyval_elt_free(elt);

	if (elt != last)
		memcpy(elt, last, sizeof(*elt));

	keyval->len--;

	return 0;
}

int ec_keyval_set(struct ec_keyval *keyval, const char *key, void *val,
	ec_keyval_elt_free_t free_cb)
{
	struct ec_keyval_elt *elt, *new_vec;

	assert(keyval != NULL);
	assert(key != NULL);

	ec_keyval_del(keyval, key);

	new_vec = ec_realloc(keyval->vec,
		sizeof(*keyval->vec) * (keyval->len + 1));
	if (new_vec == NULL)
		goto fail;

	keyval->vec = new_vec;

	elt = &new_vec[keyval->len];
	elt->key = ec_strdup(key);
	if (elt->key == NULL)
		goto fail;

	elt->val = val;
	elt->free = free_cb;
	keyval->len++;

	return 0;

fail:
	if (free_cb)
		free_cb(val);
	return -ENOMEM;
}

void ec_keyval_free(struct ec_keyval *keyval)
{
	struct ec_keyval_elt *elt;
	size_t i;

	if (keyval == NULL)
		return;

	for (i = 0; i < ec_keyval_len(keyval); i++) {
		elt = &keyval->vec[i];
		ec_keyval_elt_free(elt);
	}
	ec_free(keyval->vec);
	ec_free(keyval);
}

size_t ec_keyval_len(const struct ec_keyval *keyval)
{
	return keyval->len;
}

void ec_keyval_dump(const struct ec_keyval *keyval, FILE *out)
{
	size_t i;

	if (keyval == NULL) {
		fprintf(out, "empty keyval\n");
		return;
	}

	fprintf(out, "keyval:\n");
	for (i = 0; i < ec_keyval_len(keyval); i++) {
		fprintf(out, "  %s: %p\n",
			keyval->vec[i].key,
			keyval->vec[i].val);
	}
}

/* LCOV_EXCL_START */
static int ec_keyval_testcase(void)
{
	struct ec_keyval *keyval;
	char *val;

	keyval = ec_keyval();
	if (keyval == NULL) {
		EC_LOG(EC_LOG_ERR, "cannot create keyval\n");
		return -1;
	}

	EC_TEST_ASSERT(ec_keyval_len(keyval) == 0);
	ec_keyval_set(keyval, "key1", "val1", NULL);
	ec_keyval_set(keyval, "key2", ec_strdup("val2"), ec_free2);
	EC_TEST_ASSERT(ec_keyval_len(keyval) == 2);

	val = ec_keyval_get(keyval, "key1");
	EC_TEST_ASSERT(val != NULL && !strcmp(val, "val1"));
	val = ec_keyval_get(keyval, "key2");
	EC_TEST_ASSERT(val != NULL && !strcmp(val, "val2"));
	val = ec_keyval_get(keyval, "key3");
	EC_TEST_ASSERT(val == NULL);

	ec_keyval_set(keyval, "key1", "another_val1", NULL);
	ec_keyval_set(keyval, "key2", ec_strdup("another_val2"), ec_free2);
	EC_TEST_ASSERT(ec_keyval_len(keyval) == 2);

	val = ec_keyval_get(keyval, "key1");
	EC_TEST_ASSERT(val != NULL && !strcmp(val, "another_val1"));
	val = ec_keyval_get(keyval, "key2");
	EC_TEST_ASSERT(val != NULL && !strcmp(val, "another_val2"));

	ec_keyval_del(keyval, "key1");
	EC_TEST_ASSERT(ec_keyval_len(keyval) == 1);

	ec_keyval_free(keyval);

	return 0;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_keyval_test = {
	.name = "keyval",
	.test = ec_keyval_testcase,
};

EC_TEST_REGISTER(ec_keyval_test);
