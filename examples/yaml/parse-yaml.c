/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018, Olivier MATZ <zer0@droids-corp.org>
 */

#include <stdlib.h>
#include <stdio.h>

#include <ecoli_node.h>
#include <ecoli_yaml.h>

int
main(int argc, char *argv[])
{
	struct ec_node *node = NULL;

	if (argc != 2) {
		fprintf(stderr, "Invalid args\n");
		goto fail;
	}
	node = ec_yaml_import(argv[1]);
	if (node == NULL) {
		fprintf(stderr, "Failed to parse file\n");
		goto fail;
	}
	ec_node_dump(stdout, node);
	ec_node_free(node);

	return 0;

fail:
	ec_node_free(node);
	return 1;
}
