# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016, Olivier MATZ <zer0@droids-corp.org>

ECOLI ?= $(abspath .)
include $(ECOLI)/mk/ecoli-pre.mk

# output path with trailing slash
O ?= build/

# XXX -O0
CFLAGS  = -g -O0 -Wall -Werror -W -Wextra -fPIC -Wmissing-prototypes
CFLAGS += -Ilibecoli -Ilibecoli_yaml -Ilibecoli_editline

# XXX coverage
CFLAGS += --coverage
LDFLAGS += --coverage
#  rm -rf build; rm -rf result; make && ./build/test
#  lcov -d build -c -t build/test -o test.info && genhtml -o result test.info


srcs :=
srcs += ecoli_assert.c
srcs += ecoli_complete.c
srcs += ecoli_config.c
srcs += ecoli_keyval.c
srcs += ecoli_init.c
srcs += ecoli_log.c
srcs += ecoli_malloc.c
srcs += ecoli_murmurhash.c
srcs += ecoli_strvec.c
srcs += ecoli_test.c
srcs += ecoli_node.c
srcs += ecoli_node_any.c
srcs += ecoli_node_cmd.c
srcs += ecoli_node_empty.c
srcs += ecoli_node_expr.c
srcs += ecoli_node_expr_test.c
srcs += ecoli_node_dynamic.c
srcs += ecoli_node_file.c
srcs += ecoli_node_helper.c
srcs += ecoli_node_int.c
srcs += ecoli_node_many.c
srcs += ecoli_node_none.c
srcs += ecoli_node_once.c
srcs += ecoli_node_option.c
srcs += ecoli_node_or.c
srcs += ecoli_node_re.c
srcs += ecoli_node_re_lex.c
srcs += ecoli_node_seq.c
srcs += ecoli_node_sh_lex.c
srcs += ecoli_node_space.c
srcs += ecoli_node_str.c
srcs += ecoli_node_subset.c
srcs += ecoli_parse.c
srcs += ecoli_string.c
srcs += ecoli_vec.c

# libs
shlib-y-$(O)libecoli.so := $(addprefix libecoli/,$(srcs))

cflags-$(O)libecoli_yaml.so = -Ilibecoli_yaml
shlib-y-$(O)libecoli_yaml.so := libecoli_yaml/ecoli_yaml.c

cflags-$(O)libecoli_editline.so = -Ilibecoli_editline
shlib-y-$(O)libecoli_editline.so := libecoli_editline/ecoli_editline.c

# tests
ldflags-$(O)test = -rdynamic
exe-y-$(O)test = $(addprefix libecoli/,$(srcs)) test/test.c

# examples
ldflags-$(O)readline = -lreadline -ltermcap
exe-y-$(O)readline = $(addprefix libecoli/,$(srcs)) \
	examples/readline/main.c

ldflags-$(O)parse-yaml = -lyaml -ledit
exe-y-$(O)parse-yaml = $(addprefix libecoli/,$(srcs)) \
	libecoli_yaml/ecoli_yaml.c libecoli_editline/ecoli_editline.c\
	examples/yaml/parse-yaml.c

include $(ECOLI)/mk/ecoli-post.mk

all: _ecoli_all

clean: _ecoli_clean

.PHONY: clean all
