# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Olivier MATZ <zer0@droids-corp.org>

inc = include_directories('../../include')

parse_yaml_sources = [
	'parse-yaml.c',
]

ecoli_parse_yaml = executable(
	'ecoli-parse-yaml',
	parse_yaml_sources,
	include_directories : inc,
	link_with : libecoli,
	dependencies: [yaml_dep, edit_dep])
