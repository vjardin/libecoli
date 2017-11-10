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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <ecoli_malloc.h>
#include <ecoli_strvec.h>
#include <ecoli_test.h>
#include <ecoli_log.h>

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

struct ec_strvec *ec_strvec_ndup(const struct ec_strvec *strvec, size_t off,
	size_t len)
{
	struct ec_strvec *copy = NULL;
	size_t i;

	if (off + len > ec_strvec_len(strvec)) {
		errno = EINVAL;
		return NULL;
	}

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
		copy->len++;
	}

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
	if (strvec == NULL)
		return 0;

	return strvec->len;
}

const char *ec_strvec_val(const struct ec_strvec *strvec, size_t idx)
{
	if (strvec == NULL || idx >= strvec->len)
		return NULL;

	return strvec->vec[idx]->str;
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

	ec_strvec_dump(stdout, strvec);
	ec_strvec_dump(stdout, NULL);

	ec_strvec_free(strvec);

	return 0;

fail:
	ec_strvec_free(strvec);
	ec_strvec_free(strvec2);
	return -1;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_str_test = {
	.name = "strvec",
	.test = ec_strvec_testcase,
};

EC_TEST_REGISTER(ec_node_str_test);
