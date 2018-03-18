# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016, Olivier MATZ <zer0@droids-corp.org>

ECOLI ?= $(abspath ..)
include $(ECOLI)/mk/ecoli-pre.mk

# output path with trailing slash
O ?= build/

# XXX -O0
CFLAGS  = -g -O3 -Wall -Werror -W -Wextra -fPIC -Wmissing-prototypes
CFLAGS += -I.

# XXX coverage
CFLAGS += --coverage -fprofile-arcs -ftest-coverage
LDFLAGS += --coverage
#  rm -rf build; rm -rf result; make && ./build/test
#  lcov -d build -c -t build/test -o test.info && genhtml -o result test.info


srcs :=
srcs += ecoli_assert.c
srcs += ecoli_complete.c
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
srcs += ecoli_node_weakref.c
srcs += ecoli_parse.c
srcs += ecoli_string.c
srcs += ecoli_vec.c

shlib-y-$(O)libecoli.so := $(srcs)

ldflags-$(O)test = -rdynamic
exe-y-$(O)test = $(srcs) main.c

ldflags-$(O)readline = -lreadline -ltermcap
exe-y-$(O)readline = $(srcs) main-readline.c

include $(ECOLI)/mk/ecoli-post.mk

all: _ecoli_all

clean: _ecoli_clean

.PHONY: clean all
