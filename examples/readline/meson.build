# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Olivier MATZ <zer0@droids-corp.org>

readline_sources = [
	'main.c',
]

c_compiler = meson.get_compiler('c')
readline = c_compiler.find_library('readline', required: false)
if readline.found()
	ecoli_readline = executable(
		'ecoli-readline',
		readline_sources,
		include_directories : inc,
		link_with : libecoli,
		dependencies: readline)
endif
