# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Olivier MATZ <zer0@droids-corp.org>

inc = include_directories('../include')

libecoli_sources = [
	'ecoli_assert.c',
	'ecoli_complete.c',
	'ecoli_config.c',
	'ecoli_init.c',
	'ecoli_keyval.c',
	'ecoli_log.c',
	'ecoli_malloc.c',
	'ecoli_murmurhash.c',
	'ecoli_node_any.c',
	'ecoli_node.c',
	'ecoli_node_cmd.c',
#	'ecoli_node_cond.c',
	'ecoli_node_dynamic.c',
	'ecoli_node_empty.c',
	'ecoli_node_expr.c',
	'ecoli_node_expr_test.c',
	'ecoli_node_file.c',
	'ecoli_node_helper.c',
	'ecoli_node_int.c',
	'ecoli_node_many.c',
	'ecoli_node_none.c',
	'ecoli_node_once.c',
	'ecoli_node_option.c',
	'ecoli_node_or.c',
	'ecoli_node_re.c',
	'ecoli_node_re_lex.c',
	'ecoli_node_seq.c',
	'ecoli_node_sh_lex.c',
	'ecoli_node_space.c',
	'ecoli_node_str.c',
	'ecoli_node_subset.c',
	'ecoli_parse.c',
	'ecoli_string.c',
	'ecoli_strvec.c',
	'ecoli_test.c',
	'ecoli_vec.c',
]
deps = []
if get_option('with_yaml')
	libecoli_sources += [
		'ecoli_yaml.c',
	]
	deps += [
		yaml_dep,
	]
endif
if get_option('with_editline')
	libecoli_sources += [
		'ecoli_editline.c',
	]
	deps += [
		edit_dep,
	]
endif

libecoli = shared_library('ecoli',
	libecoli_sources,
	include_directories : inc,
	dependencies : deps,
	install : true)
