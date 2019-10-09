# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Olivier MATZ <zer0@droids-corp.org>

project('libecoli',
	'c',
	version : '0.1',
	license : 'BSD-3-clause',
	default_options: ['warning_level=2'])

edit_dep = dependency('libedit', method: 'pkg-config')
yaml_dep = dependency('yaml-0.1', method: 'pkg-config')

# XXX if debug
add_global_arguments('-Werror', language : 'c')

inc = include_directories('include')
priv_inc = include_directories('src')

subdir('src')
subdir('test')
subdir('examples')
subdir('doc')

pkg_mod = import('pkgconfig')
pkg_mod.generate(libraries : libecoli,
	version: '0.1',
	name : 'libecoli',
	filebase : 'libecoli',
	description : 'Extensible command line library.')
