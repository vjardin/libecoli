# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015, Olivier MATZ <zer0@droids-corp.org>

# core tools
CP ?= cp
LN ?= ln
GAWK ?= gawk
GREP ?= grep
# compiler and binutils, set_default overrides mk implicit value
# but not command line or standard variables
$(call set_default,CC,$(CROSS)gcc)
$(call set_default,CPP,$(CROSS)cpp)
$(call set_default,AR,$(CROSS)ar)
$(call set_default,LD,$(CROSS)ld)
$(call set_default,OBJCOPY,$(CROSS)objcopy)
$(call set_default,OBJDUMP,$(CROSS)objdump)
$(call set_default,STRIP,$(CROSS)strip)
HOSTCC ?= cc

CFLAGS += $(EXTRA_CFLAGS)
CPPFLAGS += $(EXTRA_CPPFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)
LDLIBS += $(EXTRA_LDLIBS)
