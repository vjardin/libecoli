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

#define _GNU_SOURCE /* for asprintf */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>

#include <ecoli_log.h>
#include <ecoli_malloc.h>
#include <ecoli_test.h>
#include <ecoli_strvec.h>
#include <ecoli_node.h>
#include <ecoli_parsed.h>
#include <ecoli_completed.h>
#include <ecoli_node_file.h>

struct ec_node_file {
	struct ec_node gen;
};

static int
ec_node_file_parse(const struct ec_node *gen_node,
		struct ec_parsed *state,
		const struct ec_strvec *strvec)
{
	(void)gen_node;
	(void)state;

	if (ec_strvec_len(strvec) == 0)
		return EC_PARSED_NOMATCH;

	return 1;
}

/*
 * Almost the same than dirname (3) and basename (3) except that:
 * - it always returns a substring of the given path, which can
 *   be empty.
 * - the behavior is different when the path finishes with a '/'
 * - the path argument is not modified
 * - the outputs are allocated and must be freed with ec_free().
 *
 *   path       dirname   basename       split_path
 *   /usr/lib   /usr      lib          /usr/     lib
 *   /usr/      /         usr          /usr/
 *   usr        .         usr                    usr
 *   /          /         /            /
 *   .          .         .                      .
 *   ..         .         ..                     ..
 */
static int split_path(const char *path, char **dname_p, char **bname_p)
{
	char *last_slash;
	size_t dirlen;
	char *dname, *bname;

	*dname_p = NULL;
	*bname_p = NULL;

	last_slash = strrchr(path, '/');
	if (last_slash == NULL)
		dirlen = 0;
	else
		dirlen = last_slash - path + 1;

	dname = ec_strdup(path);
	if (dname == NULL)
		return -ENOMEM;
	dname[dirlen] = '\0';

	bname = ec_strdup(path + dirlen);
	if (bname == NULL) {
		ec_free(dname);
		return -ENOMEM;
	}

	*dname_p = dname;
	*bname_p = bname;

	return 0;
}

static int
ec_node_file_complete(const struct ec_node *gen_node,
		struct ec_completed *completed,
		struct ec_parsed *state,
		const struct ec_strvec *strvec)
{
	struct stat st;
	const char *path;
	size_t bname_len;
	struct dirent *de = NULL;
	DIR *dir = NULL;
	char *dname = NULL, *bname = NULL, *effective_dir;
	char *add = NULL;
	int ret;
	int is_dir = 0;

	/*
	 * Example with this file tree:
	 * /
	 * ├── dir1
	 * │   ├── file1
	 * │   ├── file2
	 * │   └── subdir
	 * │       └── file3
	 * ├── dir2
	 * │   └── file4
	 * └── file5
	 *
	 * Input     Output completions
	 *   /       [dir1/, dir2/, file5]
	 *   /d      [dir1/, dir2/]
	 *   /f      [file5]
	 *   /dir1/  [file1, file2, subdir/]
	 *
	 *
	 *
	 */

	if (ec_strvec_len(strvec) != 1)
		goto out;

	path = ec_strvec_val(strvec, 0);
	ret = split_path(path, &dname, &bname);
	if (ret < 0) {
		ec_completed_free(completed);
		completed = NULL;
		goto out;
	}

	if (strcmp(dname, "") == 0)
		effective_dir = ".";
	else
		effective_dir = dname;

	ret = lstat(effective_dir, &st);
	if (ret != 0) {
		ret = -errno;
		goto out;
	}
	if (!S_ISDIR(st.st_mode))
		goto out;

	dir = opendir(effective_dir);
	if (dir == NULL)
		goto out;

	bname_len = strlen(bname);
	while (1) {
		de = readdir(dir);
		if (de == NULL)
			goto out;

		if (strncmp(bname, de->d_name, bname_len))
			continue;
		if (bname[0] != '.' && de->d_name[0] == '.')
			continue;

		/* add '/' if it's a dir */
		if (de->d_type == DT_DIR) {
			is_dir = 1;
		} else if (de->d_type == DT_UNKNOWN) { // XXX todo
		} else {
			is_dir = 0;
		}

		if (is_dir) {
			if (asprintf(&add, "%s%s/", path,
					&de->d_name[bname_len]) < 0) {
				ret = -errno;
				goto out;
			}
			if (ec_completed_add_partial_match(
					completed, state, gen_node, add) < 0) {
				ec_completed_free(completed);
				completed = NULL;
				goto out;
			}
		} else {
			if (asprintf(&add, "%s%s", path,
					&de->d_name[bname_len]) < 0) {
				ret = -errno;
				goto out;
			}
			if (ec_completed_add_match(completed, state, gen_node,
							add) < 0) {
				ec_completed_free(completed);
				completed = NULL;
				goto out;
			}
		}
	}
	ret = 0;

out:
	free(add);
	ec_free(dname);
	ec_free(bname);
	if (dir != NULL)
		closedir(dir);

	return ret;
}

static struct ec_node_type ec_node_file_type = {
	.name = "file",
	.parse = ec_node_file_parse,
	.complete = ec_node_file_complete,
	.size = sizeof(struct ec_node_file),
};

EC_NODE_TYPE_REGISTER(ec_node_file_type);

/* LCOV_EXCL_START */
static int ec_node_file_testcase(void)
{
	struct ec_node *node;
	int ret = 0;

	node = ec_node("file", NULL);
	if (node == NULL) {
		ec_log(EC_LOG_ERR, "cannot create node\n");
		return -1;
	}
	/* any string matches */
	ret |= EC_TEST_CHECK_PARSE(node, 1, "foo");
	ret |= EC_TEST_CHECK_PARSE(node, 1, "/tmp/bar");
	ret |= EC_TEST_CHECK_PARSE(node, -1);

	/* test completion */
#if 0 // XXX how to properly test file completion?
	ret |= EC_TEST_CHECK_COMPLETE(node,
		EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"/", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"/tmp", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"/tmp/", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
	ret |= EC_TEST_CHECK_COMPLETE(node,
		"/tmp/.", EC_NODE_ENDLIST,
		EC_NODE_ENDLIST);
#endif
	ec_node_free(node);

	return ret;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_node_file_test = {
	.name = "node_file",
	.test = ec_node_file_testcase,
};

EC_TEST_REGISTER(ec_node_file_test);
