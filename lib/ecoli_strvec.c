/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#define _GNU_SOURCE /* qsort_r */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_test.h>
#include <ecoli_log.h>
#include <ecoli_strvec.h>

EC_LOG_TYPE_REGISTER(strvec);

struct ec_strvec_elt {
	unsigned int refcnt;
	char *str;
};

struct ec_strvec {
	size_t len;
	struct ec_strvec_elt **vec;
};

struct ec_strvec *ec_strvec(void)
{
	struct ec_strvec *strvec;

	strvec = ec_calloc(1, sizeof(*strvec));
	if (strvec == NULL)
		return NULL;

	return strvec;
}

int ec_strvec_add(struct ec_strvec *strvec, const char *s)
{
	struct ec_strvec_elt *elt, **new_vec;

	new_vec = ec_realloc(strvec->vec,
		sizeof(*strvec->vec) * (strvec->len + 1));
	if (new_vec == NULL)
		return -1;

	strvec->vec = new_vec;

	elt = ec_malloc(sizeof(*elt));
	if (elt == NULL)
		return -1;

	elt->str = ec_strdup(s);
	if (elt->str == NULL) {
		ec_free(elt);
		return -1;
	}
	elt->refcnt = 1;

	new_vec[strvec->len] = elt;
	strvec->len++;
	return 0;
}

struct ec_strvec *ec_strvec_from_array(const char * const *strarr,
	size_t n)
{
	struct ec_strvec *strvec = NULL;
	size_t i;

	strvec = ec_strvec();
	if (strvec == NULL)
		goto fail;

	for (i = 0; i < n; i++) {
		if (ec_strvec_add(strvec, strarr[i]) < 0)
			goto fail;
	}

	return strvec;

fail:
	ec_strvec_free(strvec);
	return NULL;
}

int ec_strvec_del_last(struct ec_strvec *strvec)
{
	struct ec_strvec_elt *elt;

	if (strvec->len == 0) {
		errno = EINVAL;
		return -1;
	}

	elt = strvec->vec[strvec->len - 1];
	elt->refcnt--;
	if (elt->refcnt == 0) {
		ec_free(elt->str);
		ec_free(elt);
	}
	strvec->len--;
	return 0;
}

struct ec_strvec *ec_strvec_ndup(const struct ec_strvec *strvec, size_t off,
	size_t len)
{
	struct ec_strvec *copy = NULL;
	size_t i, veclen;

	veclen = ec_strvec_len(strvec);
	if (off + len > veclen)
		return NULL;

	copy = ec_strvec();
	if (copy == NULL)
		goto fail;

	if (len == 0)
		return copy;

	copy->vec = ec_calloc(len, sizeof(*copy->vec));
	if (copy->vec == NULL)
		goto fail;

	for (i = 0; i < len; i++) {
		copy->vec[i] = strvec->vec[i + off];
		copy->vec[i]->refcnt++;
	}
	copy->len = len;

	return copy;

fail:
	ec_strvec_free(copy);
	return NULL;
}

struct ec_strvec *ec_strvec_dup(const struct ec_strvec *strvec)
{
	return ec_strvec_ndup(strvec, 0, ec_strvec_len(strvec));
}

void ec_strvec_free(struct ec_strvec *strvec)
{
	struct ec_strvec_elt *elt;
	size_t i;

	if (strvec == NULL)
		return;

	for (i = 0; i < ec_strvec_len(strvec); i++) {
		elt = strvec->vec[i];
		elt->refcnt--;
		if (elt->refcnt == 0) {
			ec_free(elt->str);
			ec_free(elt);
		}
	}

	ec_free(strvec->vec);
	ec_free(strvec);
}

size_t ec_strvec_len(const struct ec_strvec *strvec)
{
	return strvec->len;
}

const char *ec_strvec_val(const struct ec_strvec *strvec, size_t idx)
{
	if (strvec == NULL || idx >= strvec->len)
		return NULL;

	return strvec->vec[idx]->str;
}

int ec_strvec_cmp(const struct ec_strvec *strvec1,
		const struct ec_strvec *strvec2)
{
	size_t i;

	if (ec_strvec_len(strvec1) != ec_strvec_len(strvec2))
		return -1;

	for (i = 0; i < ec_strvec_len(strvec1); i++) {
		if (strcmp(ec_strvec_val(strvec1, i),
				ec_strvec_val(strvec2, i)))
			return -1;
	}

	return 0;
}

static int
cmp_vec_elt(const void *p1, const void *p2, void *arg)
{
	int (*str_cmp)(const char *s1, const char *s2) = arg;
	const struct ec_strvec_elt * const *e1 = p1, * const *e2 = p2;

	return str_cmp((*e1)->str, (*e2)->str);
}

void ec_strvec_sort(struct ec_strvec *strvec,
	int (*str_cmp)(const char *s1, const char *s2))
{
	if (str_cmp == NULL)
		str_cmp = strcmp;
	qsort_r(strvec->vec, ec_strvec_len(strvec),
		sizeof(*strvec->vec), cmp_vec_elt, str_cmp);
}

void ec_strvec_dump(FILE *out, const struct ec_strvec *strvec)
{
	size_t i;

	if (strvec == NULL) {
		fprintf(out, "none\n");
		return;
	}

	fprintf(out, "strvec (len=%zu) [", strvec->len);
	for (i = 0; i < ec_strvec_len(strvec); i++) {
		if (i == 0)
			fprintf(out, "%s", strvec->vec[i]->str);
		else
			fprintf(out, ", %s", strvec->vec[i]->str);
	}
	fprintf(out, "]\n");

}

/* LCOV_EXCL_START */
static int ec_strvec_testcase(void)
{
	struct ec_strvec *strvec = NULL;
	struct ec_strvec *strvec2 = NULL;
	FILE *f = NULL;
	char *buf = NULL;
	size_t buflen = 0;
	int testres = 0;

	strvec = ec_strvec();
	if (strvec == NULL) {
		EC_TEST_ERR("cannot create strvec\n");
		goto fail;
	}
	if (ec_strvec_len(strvec) != 0) {
		EC_TEST_ERR("bad strvec len (0)\n");
		goto fail;
	}
	if (ec_strvec_add(strvec, "0") < 0) {
		EC_TEST_ERR("cannot add (0) in strvec\n");
		goto fail;
	}
	if (ec_strvec_len(strvec) != 1) {
		EC_TEST_ERR("bad strvec len (1)\n");
		goto fail;
	}
	if (ec_strvec_add(strvec, "1") < 0) {
		EC_TEST_ERR("cannot add (1) in strvec\n");
		goto fail;
	}
	if (ec_strvec_len(strvec) != 2) {
		EC_TEST_ERR("bad strvec len (2)\n");
		goto fail;
	}
	if (strcmp(ec_strvec_val(strvec, 0), "0")) {
		EC_TEST_ERR("invalid element in strvec (0)\n");
		goto fail;
	}
	if (strcmp(ec_strvec_val(strvec, 1), "1")) {
		EC_TEST_ERR("invalid element in strvec (1)\n");
		goto fail;
	}
	if (ec_strvec_val(strvec, 2) != NULL) {
		EC_TEST_ERR("strvec val should be NULL\n");
		goto fail;
	}

	strvec2 = ec_strvec_dup(strvec);
	if (strvec2 == NULL) {
		EC_TEST_ERR("cannot create strvec2\n");
		goto fail;
	}
	if (ec_strvec_len(strvec2) != 2) {
		EC_TEST_ERR("bad strvec2 len (2)\n");
		goto fail;
	}
	if (strcmp(ec_strvec_val(strvec2, 0), "0")) {
		EC_TEST_ERR("invalid element in strvec2 (0)\n");
		goto fail;
	}
	if (strcmp(ec_strvec_val(strvec2, 1), "1")) {
		EC_TEST_ERR("invalid element in strvec2 (1)\n");
		goto fail;
	}
	if (ec_strvec_val(strvec2, 2) != NULL) {
		EC_TEST_ERR("strvec2 val should be NULL\n");
		goto fail;
	}
	ec_strvec_free(strvec2);

	strvec2 = ec_strvec_ndup(strvec, 0, 0);
	if (strvec2 == NULL) {
		EC_TEST_ERR("cannot create strvec2\n");
		goto fail;
	}
	if (ec_strvec_len(strvec2) != 0) {
		EC_TEST_ERR("bad strvec2 len (0)\n");
		goto fail;
	}
	if (ec_strvec_val(strvec2, 0) != NULL) {
		EC_TEST_ERR("strvec2 val should be NULL\n");
		goto fail;
	}
	ec_strvec_free(strvec2);

	strvec2 = ec_strvec_ndup(strvec, 1, 1);
	if (strvec2 == NULL) {
		EC_TEST_ERR("cannot create strvec2\n");
		goto fail;
	}
	if (ec_strvec_len(strvec2) != 1) {
		EC_TEST_ERR("bad strvec2 len (1)\n");
		goto fail;
	}
	if (strcmp(ec_strvec_val(strvec2, 0), "1")) {
		EC_TEST_ERR("invalid element in strvec2 (1)\n");
		goto fail;
	}
	if (ec_strvec_val(strvec2, 1) != NULL) {
		EC_TEST_ERR("strvec2 val should be NULL\n");
		goto fail;
	}
	ec_strvec_free(strvec2);

	strvec2 = ec_strvec_ndup(strvec, 3, 1);
	if (strvec2 != NULL) {
		EC_TEST_ERR("strvec2 should be NULL\n");
		goto fail;
	}
	ec_strvec_free(strvec2);

	strvec2 = EC_STRVEC("0", "1");
	if (strvec2 == NULL) {
		EC_TEST_ERR("cannot create strvec from array\n");
		goto fail;
	}
	testres |= EC_TEST_CHECK(ec_strvec_cmp(strvec, strvec2) == 0,
		"strvec and strvec2 should be equal\n");
	ec_strvec_free(strvec2);

	f = open_memstream(&buf, &buflen);
	if (f == NULL)
		goto fail;
	ec_strvec_dump(f, strvec);
	fclose(f);
	f = NULL;
	testres |= EC_TEST_CHECK(
		strstr(buf, "strvec (len=2) [0, 1]"), "bad dump\n");
	free(buf);
	buf = NULL;

	ec_strvec_del_last(strvec);
	strvec2 = EC_STRVEC("0");
	if (strvec2 == NULL) {
		EC_TEST_ERR("cannot create strvec from array\n");
		goto fail;
	}
	testres |= EC_TEST_CHECK(ec_strvec_cmp(strvec, strvec2) == 0,
		"strvec and strvec2 should be equal\n");
	ec_strvec_free(strvec2);
	strvec2 = NULL;

	f = open_memstream(&buf, &buflen);
	if (f == NULL)
		goto fail;
	ec_strvec_dump(f, NULL);
	fclose(f);
	f = NULL;
	testres |= EC_TEST_CHECK(
		strstr(buf, "none"), "bad dump\n");
	free(buf);
	buf = NULL;

	ec_strvec_free(strvec);

	strvec = EC_STRVEC("e", "a", "f", "d", "b", "c");
	if (strvec == NULL) {
		EC_TEST_ERR("cannot create strvec from array\n");
		goto fail;
	}
	ec_strvec_sort(strvec, NULL);
	strvec2 = EC_STRVEC("a", "b", "c", "d", "e", "f");
	if (strvec2 == NULL) {
		EC_TEST_ERR("cannot create strvec from array\n");
		goto fail;
	}
	testres |= EC_TEST_CHECK(ec_strvec_cmp(strvec, strvec2) == 0,
		"strvec and strvec2 should be equal\n");
	ec_strvec_free(strvec2);
	strvec = NULL;
	ec_strvec_free(strvec);
	strvec2 = NULL;

	return testres;

fail:
	if (f != NULL)
		fclose(f);
	ec_strvec_free(strvec);
	ec_strvec_free(strvec2);
	free(buf);

	return -1;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_str_test = {
	.name = "strvec",
	.test = ec_strvec_testcase,
};

EC_TEST_REGISTER(ec_node_str_test);
