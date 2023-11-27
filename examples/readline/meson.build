# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Olivier MATZ <zer0@droids-corp.org>

readline_sources = files(
	'main.c',
)

ecoli_readline = executable(
	'ecoli-readline',
	readline_sources,
	include_directories : inc,
	link_with : libecoli,
	dependencies: readline_dep)
