#
# Copyright 2015, Olivier MATZ <zer0@droids-corp.org>
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
#

# ---- variables that must be defined:
#
#  ECOLI: path to ecoli root
#
# ---- variable that can be defined anywhere
#
#  CROSS: prefix of the toolchain
#  CP, LN, GAWK, GREP: coreutils tools
#  CC, CPP, AR, LD, OBJCOPY, OBJDUMP, STRIP: compilers/binutils
#
# ---- variable that can be defined by Makefile:
#
#  obj-y-$(path)
#  exe-y-$(path)
#  ar-y-$(path)
#  shlib-y-$(path)
#  copy-y-$(path)
#  slink-y-$(path)
#  objcopy-y-$(path)
#  subdir-y
#
#  CPPFLAGS, CFLAGS, LDFLAGS, LDLIBS: global flags
#  cflags-$(path), cppflags-$(path), ldflags-$(path), ldlibs-$(path): per
#    file flags
#  mkflags-$(path): flags for subdirectories
#
# ---- variables that can be defined on the command line:
#
#  EXTRA_CPPFLAGS, EXTRA_CFLAGS, EXTRA_LDFLAGS, EXTRA_LDLIBS: global
#    extra flags
#  extra-cflags-$(path), extra-cppflags-$(path): per object extra flags

ifeq ($(ECOLI),)
$(error ECOLI environment variable is not defined)
endif

# list of targets asked by user
all-targets :=
# list of files generated
all-clean-file :=

# usual internal variables:
#   out-$(file) = output path of a generated file
#   pre-$(file) = list of files needed to generate $(file)
#   all-type = list of targets for this type

include $(ECOLI)/mk/ecoli-obj-vars.mk
include $(ECOLI)/mk/ecoli-exe-vars.mk
include $(ECOLI)/mk/ecoli-ar-vars.mk
include $(ECOLI)/mk/ecoli-shlib-vars.mk
include $(ECOLI)/mk/ecoli-copy-vars.mk
include $(ECOLI)/mk/ecoli-slink-vars.mk
include $(ECOLI)/mk/ecoli-objcopy-vars.mk
include $(ECOLI)/mk/ecoli-subdir-vars.mk
# must stay at the end
include $(ECOLI)/mk/ecoli-clean-vars.mk

# dump the list of targets
ifeq ($(D),1)
$(call disp_list,------ all-targets,$(all-targets))
endif

# first rule (default)
.PHONY: _ecoli_all
_ecoli_all: $(all-targets)

# the includes below require second expansion
.SECONDEXPANSION:

include $(ECOLI)/mk/ecoli-obj-rules.mk
include $(ECOLI)/mk/ecoli-exe-rules.mk
include $(ECOLI)/mk/ecoli-ar-rules.mk
include $(ECOLI)/mk/ecoli-shlib-rules.mk
include $(ECOLI)/mk/ecoli-copy-rules.mk
include $(ECOLI)/mk/ecoli-slink-rules.mk
include $(ECOLI)/mk/ecoli-objcopy-rules.mk
include $(ECOLI)/mk/ecoli-subdir-rules.mk
include $(ECOLI)/mk/ecoli-clean-rules.mk

.PHONY: FORCE
FORCE:
