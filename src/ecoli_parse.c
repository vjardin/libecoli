/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <ecoli_assert.h>
#include <ecoli_malloc.h>
#include <ecoli_strvec.h>
#include <ecoli_dict.h>
#include <ecoli_log.h>
#include <ecoli_test.h>
#include <ecoli_node.h>
#include <ecoli_node_sh_lex.h>
#include <ecoli_node_str.h>
#include <ecoli_node_seq.h>
#include <ecoli_parse.h>

EC_LOG_TYPE_REGISTER(parse);

TAILQ_HEAD(ec_pnode_list, ec_pnode);

struct ec_pnode {
	TAILQ_ENTRY(ec_pnode) next;
	struct ec_pnode_list children;
	struct ec_pnode *parent;
	const struct ec_node *node;
	struct ec_strvec *strvec;
	struct ec_dict *attrs;
};

static int __ec_parse_child(const struct ec_node *node,
				struct ec_pnode *pstate,
				bool is_root, const struct ec_strvec *strvec)
{
	struct ec_strvec *match_strvec;
	struct ec_pnode *child = NULL;
	int ret;

	// XXX limit max number of recursions to avoid segfault

	if (ec_node_type(node)->parse == NULL) {
		errno = ENOTSUP;
		return -1;
	}

	if (!is_root) {
		child = ec_pnode(node);
		if (child == NULL)
			return -1;

		ec_pnode_link_child(pstate, child);
	} else {
		child = pstate;
	}
	ret = ec_node_type(node)->parse(node, child, strvec);
	if (ret < 0)
		goto fail;

	if (ret == EC_PARSE_NOMATCH) {
		if (!is_root) {
			ec_pnode_unlink_child(pstate, child);
			ec_pnode_free(child);
		}
		return ret;
	}

	match_strvec = ec_strvec_ndup(strvec, 0, ret);
	if (match_strvec == NULL)
		goto fail;

	child->strvec = match_strvec;

	return ret;

fail:
	if (!is_root) {
		ec_pnode_unlink_child(pstate, child);
		ec_pnode_free(child);
	}
	return -1;
}

int ec_parse_child(const struct ec_node *node, struct ec_pnode *pstate,
			const struct ec_strvec *strvec)
{
	assert(pstate != NULL);
	return __ec_parse_child(node, pstate, false, strvec);
}

// XXX what is returned if no match ??
struct ec_pnode *ec_parse_strvec(const struct ec_node *node,
				const struct ec_strvec *strvec)
{
	struct ec_pnode *parse = ec_pnode(node);
	int ret;

	if (parse == NULL)
		return NULL;

	ret = __ec_parse_child(node, parse, true, strvec);
	if (ret < 0) {
		ec_pnode_free(parse);
		return NULL;
	}

	return parse;
}

struct ec_pnode *ec_parse(const struct ec_node *node, const char *str)
{
	struct ec_strvec *strvec = NULL;
	struct ec_pnode *parse = NULL;

	errno = ENOMEM;
	strvec = ec_strvec();
	if (strvec == NULL)
		goto fail;

	if (ec_strvec_add(strvec, str) < 0)
		goto fail;

	parse = ec_parse_strvec(node, strvec);
	if (parse == NULL)
		goto fail;

	ec_strvec_free(strvec);
	return parse;

 fail:
	ec_strvec_free(strvec);
	ec_pnode_free(parse);
	return NULL;
}

struct ec_pnode *ec_pnode(const struct ec_node *node)
{
	struct ec_pnode *parse = NULL;

	parse = ec_calloc(1, sizeof(*parse));
	if (parse == NULL)
		goto fail;

	TAILQ_INIT(&parse->children);

	parse->node = node;
	parse->attrs = ec_dict();
	if (parse->attrs == NULL)
		goto fail;

	return parse;

 fail:
	if (parse != NULL)
		ec_dict_free(parse->attrs);
	ec_free(parse);

	return NULL;
}

static struct ec_pnode *
__ec_pnode_dup(const struct ec_pnode *root, const struct ec_pnode *ref,
		struct ec_pnode **new_ref)
{
	struct ec_pnode *dup = NULL;
	struct ec_pnode *child, *dup_child;
	struct ec_dict *attrs = NULL;

	if (root == NULL)
		return NULL;

	dup = ec_pnode(root->node);
	if (dup == NULL)
		return NULL;

	if (root == ref)
		*new_ref = dup;

	attrs = ec_dict_dup(root->attrs);
	if (attrs == NULL)
		goto fail;
	ec_dict_free(dup->attrs);
	dup->attrs = attrs;

	if (root->strvec != NULL) {
		dup->strvec = ec_strvec_dup(root->strvec);
		if (dup->strvec == NULL)
			goto fail;
	}

	TAILQ_FOREACH(child, &root->children, next) {
		dup_child = __ec_pnode_dup(child, ref, new_ref);
		if (dup_child == NULL)
			goto fail;
		ec_pnode_link_child(dup, dup_child);
	}

	return dup;

fail:
	ec_pnode_free(dup);
	return NULL;
}

struct ec_pnode *ec_pnode_dup(const struct ec_pnode *parse)
{
	const struct ec_pnode *root;
	struct ec_pnode *dup_root, *dup = NULL;

	root = ec_pnode_get_root(parse);
	dup_root = __ec_pnode_dup(root, parse, &dup);
	if (dup_root == NULL)
		return NULL;
	assert(dup != NULL);

	return dup;
}

void ec_pnode_free_children(struct ec_pnode *parse)
{
	struct ec_pnode *child;

	if (parse == NULL)
		return;

	while (!TAILQ_EMPTY(&parse->children)) {
		child = TAILQ_FIRST(&parse->children);
		TAILQ_REMOVE(&parse->children, child, next);
		child->parent = NULL;
		ec_pnode_free(child);
	}
}

void ec_pnode_free(struct ec_pnode *parse)
{
	if (parse == NULL)
		return;

	ec_assert_print(parse->parent == NULL,
			"parent not NULL in ec_pnode_free()");

	ec_pnode_free_children(parse);
	ec_strvec_free(parse->strvec);
	ec_dict_free(parse->attrs);
	ec_free(parse);
}

static void __ec_pnode_dump(FILE *out,
	const struct ec_pnode *parse, size_t indent)
{
	struct ec_pnode *child;
	const struct ec_strvec *vec;
	const char *id = "none", *typename = "none";

	/* node can be null when parsing is incomplete */
	if (parse->node != NULL) {
		id = ec_node_id(parse->node);
		typename = ec_node_type(parse->node)->name;
	}

	fprintf(out, "%*s" "type=%s id=%s vec=",
		(int)indent * 4, "", typename, id);
	vec = ec_pnode_strvec(parse);
	ec_strvec_dump(out, vec);

	TAILQ_FOREACH(child, &parse->children, next)
		__ec_pnode_dump(out, child, indent + 1);
}

void ec_pnode_dump(FILE *out, const struct ec_pnode *parse)
{
	fprintf(out, "------------------- parse dump:\n");

	if (parse == NULL) {
		fprintf(out, "parse is NULL\n");
		return;
	}

	/* only exist if it does not match (strvec == NULL) and if it
	 * does not have children: an incomplete parse, like those
	 * generated by complete() don't match but have children that
	 * may match. */
	if (!ec_pnode_matches(parse) && TAILQ_EMPTY(&parse->children)) {
		fprintf(out, "no match\n");
		return;
	}

	__ec_pnode_dump(out, parse, 0);
}

void ec_pnode_link_child(struct ec_pnode *parse,
	struct ec_pnode *child)
{
	TAILQ_INSERT_TAIL(&parse->children, child, next);
	child->parent = parse;
}

void ec_pnode_unlink_child(struct ec_pnode *parse,
	struct ec_pnode *child)
{
	TAILQ_REMOVE(&parse->children, child, next);
	child->parent = NULL;
}

struct ec_pnode *
ec_pnode_get_first_child(const struct ec_pnode *parse)
{
	return TAILQ_FIRST(&parse->children);
}

struct ec_pnode *
ec_pnode_get_last_child(const struct ec_pnode *parse)
{
	return TAILQ_LAST(&parse->children, ec_pnode_list);
}

struct ec_pnode *ec_pnode_next(const struct ec_pnode *parse)
{
	return TAILQ_NEXT(parse, next);
}

bool ec_pnode_has_child(const struct ec_pnode *parse)
{
	return !TAILQ_EMPTY(&parse->children);
}

const struct ec_node *ec_pnode_get_node(const struct ec_pnode *parse)
{
	if (parse == NULL)
		return NULL;

	return parse->node;
}

void ec_pnode_del_last_child(struct ec_pnode *parse)
{
	struct ec_pnode *child;

	child = ec_pnode_get_last_child(parse);
	ec_pnode_unlink_child(parse, child);
	ec_pnode_free(child);
}

struct ec_pnode *__ec_pnode_get_root(struct ec_pnode *parse)
{
	if (parse == NULL)
		return NULL;

	while (parse->parent != NULL)
		parse = parse->parent;

	return parse;
}

struct ec_pnode *ec_pnode_get_parent(const struct ec_pnode *parse)
{
	if (parse == NULL)
		return NULL;

	return parse->parent;
}

struct ec_pnode *__ec_pnode_iter_next(const struct ec_pnode *root,
				struct ec_pnode *parse, bool iter_children)
{
	struct ec_pnode *child, *parent, *next;

	if (iter_children) {
		child = TAILQ_FIRST(&parse->children);
		if (child != NULL)
			return child;
	}
	parent = parse->parent;
	while (parent != NULL && parse != root) {
		next = TAILQ_NEXT(parse, next);
		if (next != NULL)
			return next;
		parse = parent;
		parent = parse->parent;
	}
	return NULL;
}

struct ec_pnode *
ec_pnode_find_next(struct ec_pnode *root, struct ec_pnode *start,
		const char *id, bool iter_children)
{
	struct ec_pnode *iter;

	if (root == NULL)
		return NULL;
	if (start == NULL)
		start = root;
	else
		start = EC_PNODE_ITER_NEXT(root, start, iter_children);

	for (iter = start; iter != NULL;
	     iter = EC_PNODE_ITER_NEXT(root, iter, 1)) {
		if (iter->node != NULL &&
				!strcmp(ec_node_id(iter->node), id))
			return iter;
	}

	return NULL;
}

struct ec_pnode *ec_pnode_find(struct ec_pnode *parse,
	const char *id)
{
	return ec_pnode_find_next(parse, NULL, id, 1);
}

struct ec_dict *
ec_pnode_get_attrs(struct ec_pnode *parse)
{
	if (parse == NULL)
		return NULL;

	return parse->attrs;
}

const struct ec_strvec *ec_pnode_strvec(const struct ec_pnode *parse)
{
	if (parse == NULL || parse->strvec == NULL)
		return NULL;

	return parse->strvec;
}

/* number of strings in the parse vector */
size_t ec_pnode_len(const struct ec_pnode *parse)
{
	if (parse == NULL || parse->strvec == NULL)
		return 0;

	return ec_strvec_len(parse->strvec);
}

size_t ec_pnode_matches(const struct ec_pnode *parse)
{
	if (parse == NULL)
		return 0;

	if (parse->strvec == NULL)
		return 0;

	return 1;
}

/* LCOV_EXCL_START */
static int ec_pnode_testcase(void)
{
	struct ec_node *node = NULL;
	struct ec_pnode *p = NULL, *p2 = NULL;
	const struct ec_pnode *pc;
	FILE *f = NULL;
	char *buf = NULL;
	size_t buflen = 0;
	int testres = 0;
	int ret;

	node = ec_node_sh_lex(EC_NO_ID,
			EC_NODE_SEQ(EC_NO_ID,
				ec_node_str("id_x", "x"),
				ec_node_str("id_y", "y")));
	if (node == NULL)
		goto fail;

	p = ec_parse(node, "xcdscds");
	testres |= EC_TEST_CHECK(
		p != NULL && !ec_pnode_matches(p),
		"parse should not match\n");

	f = open_memstream(&buf, &buflen);
	if (f == NULL)
		goto fail;
	ec_pnode_dump(f, p);
	fclose(f);
	f = NULL;

	testres |= EC_TEST_CHECK(
		strstr(buf, "no match"), "bad dump\n");
	free(buf);
	buf = NULL;
	ec_pnode_free(p);

	p = ec_parse(node, "x y");
	testres |= EC_TEST_CHECK(
		p != NULL && ec_pnode_matches(p),
		"parse should match\n");
	testres |= EC_TEST_CHECK(
		ec_pnode_len(p) == 1, "bad parse len\n");

	ret = ec_dict_set(ec_pnode_get_attrs(p), "key", "val", NULL);
	testres |= EC_TEST_CHECK(ret == 0,
		"cannot set parse attribute\n");

	p2 = ec_pnode_dup(p);
	testres |= EC_TEST_CHECK(
		p2 != NULL && ec_pnode_matches(p2),
		"parse should match\n");
	ec_pnode_free(p2);
	p2 = NULL;

	pc = ec_pnode_find(p, "id_x");
	testres |= EC_TEST_CHECK(pc != NULL, "cannot find id_x");
	testres |= EC_TEST_CHECK(pc != NULL &&
		ec_pnode_get_parent(pc) != NULL &&
		ec_pnode_get_parent(ec_pnode_get_parent(pc)) == p,
		"invalid parent\n");

	pc = ec_pnode_find(p, "id_y");
	testres |= EC_TEST_CHECK(pc != NULL, "cannot find id_y");
	pc = ec_pnode_find(p, "id_dezdezdez");
	testres |= EC_TEST_CHECK(pc == NULL, "should not find bad id");


	f = open_memstream(&buf, &buflen);
	if (f == NULL)
		goto fail;
	ec_pnode_dump(f, p);
	fclose(f);
	f = NULL;

	testres |= EC_TEST_CHECK(
		strstr(buf, "type=sh_lex id=no-id") &&
		strstr(buf, "type=seq id=no-id") &&
		strstr(buf, "type=str id=id_x") &&
		strstr(buf, "type=str id=id_x"),
		"bad dump\n");
	free(buf);
	buf = NULL;

	ec_pnode_free(p);
	ec_node_free(node);
	return testres;

fail:
	ec_pnode_free(p2);
	ec_pnode_free(p);
	ec_node_free(node);
	if (f != NULL)
		fclose(f);
	free(buf);

	return -1;
}
/* LCOV_EXCL_STOP */

static struct ec_test ec_pnode_test = {
	.name = "parse",
	.test = ec_pnode_testcase,
};

EC_TEST_REGISTER(ec_pnode_test);
