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
