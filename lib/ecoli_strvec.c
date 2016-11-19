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

#include <ecoli_malloc.h>
#include <ecoli_strvec.h>

struct ec_strvec *ec_strvec_new(void)
{
	struct ec_strvec *strvec;

	strvec = ec_calloc(1, sizeof(*strvec));
	if (strvec == NULL)
		return NULL;

	return strvec;
}

int ec_strvec_add(struct ec_strvec *strvec, const char *s)
{
	char **new_vec;

	new_vec = ec_realloc(strvec->vec,
		sizeof(*strvec->vec) * (strvec->len + 1));
	if (new_vec == NULL)
		return -1;

	strvec->vec = new_vec;
	strvec->vec[strvec->len] = ec_strdup(s);
	if (strvec->vec[strvec->len] == NULL)
		return -1;

	strvec->len++;
	return 0;
}

struct ec_strvec *ec_strvec_ndup(const struct ec_strvec *strvec, size_t len)
{
	struct ec_strvec *copy = NULL;
	size_t i, veclen;

	copy = ec_strvec_new();
	if (copy == NULL)
		goto fail;

	if (len == 0)
		return copy;

	veclen = ec_strvec_len(strvec);
	if (len > veclen)
		len = veclen;
	copy->vec = ec_calloc(len, sizeof(*copy->vec));
	if (copy->vec == NULL)
		goto fail;

	for (i = 0; i < len; i++) {
		copy->vec[i] = ec_strdup(strvec->vec[i]);
		if (copy->vec[i] == NULL)
			goto fail;
		copy->len++;
	}

	return copy;

fail:
	ec_strvec_free(copy);
	return NULL;
}

struct ec_strvec *ec_strvec_dup(const struct ec_strvec *strvec)
{
	return ec_strvec_ndup(strvec, ec_strvec_len(strvec));
}

void ec_strvec_free(struct ec_strvec *strvec)
{
	size_t i;

	if (strvec == NULL)
		return;

	for (i = 0; i < ec_strvec_len(strvec); i++)
		ec_free(ec_strvec_val(strvec, i));

	ec_free(strvec->vec);
	ec_free(strvec);
}

size_t ec_strvec_len(const struct ec_strvec *strvec)
{
	return strvec->len;
}

char *ec_strvec_val(const struct ec_strvec *strvec, size_t idx)
{
	if (strvec == NULL || idx >= strvec->len)
		return NULL;

	return strvec->vec[idx];
}

int ec_strvec_slice(struct ec_strvec *strvec, const struct ec_strvec *from,
	size_t off)
{
	if (off > from->len)
		return -1;

	strvec->len = from->len - off;
	strvec->vec = &from->vec[off];

	return 0;
}

void ec_strvec_dump(const struct ec_strvec *strvec, FILE *out)
{
	size_t i;

	if (strvec == NULL) {
		fprintf(out, "empty strvec\n");
		return;
	}

	fprintf(out, "strvec:\n");
	for (i = 0; i < ec_strvec_len(strvec); i++)
		fprintf(out, "  %zd: %s\n", i, strvec->vec[i]);
}
