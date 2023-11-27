# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Olivier MATZ <zer0@droids-corp.org>

doc_install_dir = join_paths('share', 'doc', 'libecoli')

cdata = configuration_data()
cdata.set('VERSION', meson.project_version())
cdata.set('OUTPUT', join_paths(meson.build_root(), 'doc', 'api'))
cdata.set('TOPDIR', meson.source_root())
doxygen = find_program('doxygen')
sphinx_build = find_program('sphinx-build')

doxygen_conf = configure_file(
  input: 'Doxyfile.in',
  output: 'Doxyfile',
  configuration: cdata,
  install: false)

doxygen_build = custom_target(
  'doxygen',
  input: doxygen_conf,
  output: 'api',
  command: [doxygen, '@INPUT@', '@OUTPUT@'],
  build_by_default: true,
  install_dir: doc_install_dir)

html_doc = custom_target('html_doc',
	input: meson.current_source_dir(),
	output: 'html_doc',
	command: [sphinx_build, '@INPUT@', '@OUTPUT@'],
	install: true,
	install_dir: doc_install_dir)
