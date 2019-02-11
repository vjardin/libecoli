# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Olivier MATZ <zer0@droids-corp.org>

inc = include_directories('../include')

test_sources = [
	'test.c',
]

ecoli_test = executable('ecoli-test',
	test_sources,
	include_directories : inc,
	link_with : libecoli)

test('libecoli test', ecoli_test)
