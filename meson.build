# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Olivier MATZ <zer0@droids-corp.org>

project('libecoli',
	'c',
	version : '0.1',
	license : 'BSD-3-clause',
	default_options: ['warning_level=2'])

edit_dep = dependency('libedit', required: get_option('with_editline'))
yaml_dep = dependency('yaml-0.1', required: get_option('with_yaml'))

add_global_arguments('-Wmissing-prototypes', language : 'c')

inc = include_directories('include')
priv_inc = include_directories('src')

subdir('src')
if get_option('build_tests')
	subdir('test')
endif
if get_option('build_examples')
	subdir('examples')
endif
if get_option('build_doc')
	subdir('doc')
endif

pkg_mod = import('pkgconfig')
pkg_mod.generate(libraries : libecoli,
	version: '0.1',
	name : 'libecoli',
	filebase : 'libecoli',
	description : 'Extensible command line library.')
