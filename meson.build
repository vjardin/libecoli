# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Olivier MATZ <zer0@droids-corp.org>

project('libecoli',
	'c',
	version : '0.1',
	license : 'BSD-3-clause',
	default_options: [
		'warning_level=2',
		'werror=true',
		'c_std=c99',
	])

edit_dep = dependency('libedit', required: get_option('with_editline'))
yaml_dep = dependency('yaml-0.1', required: get_option('with_yaml'))

add_project_arguments('-Wmissing-prototypes', language : 'c')
add_project_arguments('-D_GNU_SOURCE', language : 'c')
if get_option('build_tests')
	add_project_arguments('--coverage', language : 'c')
	add_project_link_arguments('--coverage' , language: 'c')
endif

inc = include_directories('include')
priv_inc = include_directories('src')
libecoli_sources = []

subdir('src')

if meson.is_subproject()
	libecoli = static_library('ecoli',
		libecoli_sources,
		include_directories : [inc, priv_inc],
		dependencies : deps,
		install : false
	)
	libecoli_dep = declare_dependency(
		link_whole: libecoli,
		include_directories: inc,
	)
else
	libecoli = shared_library('ecoli',
		libecoli_sources,
		include_directories : [inc, priv_inc],
		dependencies : deps,
		install : true
	)
	pkg_mod = import('pkgconfig')
	pkg_mod.generate(libraries : libecoli,
		version: '0.1',
		name : 'libecoli',
		filebase : 'libecoli',
		description : 'Extensible command line library.')
endif

if get_option('build_tests')
	subdir('test')
endif
if get_option('build_examples')
	subdir('examples')
endif
if get_option('build_doc')
	subdir('doc')
endif
