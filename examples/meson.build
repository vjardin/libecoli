# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Olivier MATZ <zer0@droids-corp.org>

readline_dep = dependency('readline', required: false)
if readline_dep.found()
	subdir('readline')
endif
if yaml_dep.found() and edit_dep.found()
	subdir('parse-yaml')
endif
