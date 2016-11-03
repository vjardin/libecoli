#
# Copyright 2016, Olivier MATZ <zer0@droids-corp.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the University of California, Berkeley nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

ECOLI ?= $(abspath ..)
include $(ECOLI)/mk/ecoli-pre.mk

# output path with trailing slash
O ?= build/

CFLAGS  = -g -O0 -Wall -Werror -W -fPIC
CFLAGS += -I.

srcs := ecoli_tk.c ecoli_tk_str.c ecoli_tk_seq.c
srcs += ecoli_tk_space.c ecoli_tk_or.c ecoli_test.c
srcs += ecoli_tk_empty.c ecoli_tk_int.c
srcs += ecoli_malloc.c ecoli_log.c ecoli_tk_option.c
srcs += ecoli_tk_shlex.c
shlib-y-$(O)libecoli.so := $(srcs)

exe-y-$(O)test = $(srcs) main.c

ldflags-$(O)readline = -lreadline
exe-y-$(O)readline = $(srcs) main-readline.c

include $(ECOLI)/mk/ecoli-post.mk

all: _ecoli_all

clean: _ecoli_clean

.PHONY: clean all
