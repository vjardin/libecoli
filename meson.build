# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Olivier MATZ <zer0@droids-corp.org>

project('libecoli',
	'c',
	version : '0.1',
	license : 'BSD-3-clause')

# if debug
add_global_arguments('-Wall', language : 'c')
add_global_arguments('-Werror', language : 'c')
add_global_arguments('-W', language : 'c')
add_global_arguments('-Wextra', language : 'c')

edit_dep = dependency('libedit', method: 'pkg-config')
yaml_dep = dependency('yaml-0.1', method: 'pkg-config')

subdir('src')
subdir('test')
subdir('examples')

pkg_mod = import('pkgconfig')
pkg_mod.generate(libraries : libecoli,
	version: '0.1',
	name : 'libecoli',
	filebase : 'libecoli',
	description : 'Extensible command line library.')
