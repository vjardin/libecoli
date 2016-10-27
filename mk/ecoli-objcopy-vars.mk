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

# objcopy changes the format of a binary
# objcopy-hex-y-$(objcopy), objcopy-bin-y-$(objcopy) are provided by the user
#   $(objcopy) is the path of the binary, and the variable contains
#   the path to the elf. Several objcopy-y-$(objcopy) can be present.

# list all executable builds requested by user
all-objcopy-hex := $(patsubst objcopy-hex-y-%,%,$(filter objcopy-hex-y-%,$(.VARIABLES)))
all-objcopy-bin := $(patsubst objcopy-bin-y-%,%,$(filter objcopy-bin-y-%,$(.VARIABLES)))

# add them to the list of targets
all-targets += $(all-objcopy-hex) $(all-objcopy-bin)

# for each objcopy, create the following variables:
#   out-$(objcopy) = output path of the executable
#   pre-$(objcopy) = list of prerequisites for this executable
# We also add the generated files in $(all-clean-file).
$(foreach objcopy,$(all-objcopy-hex),\
	$(if $(call compare,$(words $(objcopy-hex-y-$(objcopy))),1),\
		$(error "only one source file is allowed in objcopy-hex-y-$(objcopy)")) \
	$(eval out-$(objcopy) := $(dir $(objcopy))) \
	$(eval pre-$(objcopy) := $(objcopy-hex-y-$(objcopy))) \
	$(eval all-clean-file += $(objcopy)) \
)

# for each objcopy, create the following variables:
#   out-$(objcopy) = output path of the executable
#   pre-$(objcopy) = list of prerequisites for this executable
# We also add the generated files in $(all-clean-file).
$(foreach objcopy,$(all-objcopy-bin),\
	$(if $(call compare,$(words $(objcopy-bin-y-$(objcopy))),1),\
		$(error "only one source file is allowed in objcopy-bin-y-$(objcopy)")) \
	$(eval out-$(objcopy) := $(dir $(objcopy))) \
	$(eval pre-$(objcopy) := $(objcopy-bin-y-$(objcopy))) \
	$(eval all-clean-file += $(objcopy)) \
)

# convert format of executable from elf to ihex
#   $1: source executable (elf)
#   $2: destination file
objcopy_hex_cmd = $(OBJCOPY) -O ihex $(1) $(2)

# print line used to convert executable format
ifeq ($(V),1)
objcopy_print_cmd = echo $(call protect_quote,$(call objcopy_hex_cmd,$1,$2))
else
objcopy_print_cmd = echo "  OBJCOPY $(2)"
endif

# convert format of executable from elf to binary
#   $1: source executable (elf)
#   $2: destination file
objcopy_bin_cmd = $(OBJCOPY) -O binary $(1) $(2)

# print line used to convert executable format
ifeq ($(V),1)
objcopy_print_cmd = echo $(call protect_quote,$(call objcopy_bin_cmd,$1,$2))
else
objcopy_print_cmd = echo "  OBJCOPY $(2)"
endif

# XXX dup ?
all-clean-file += $(all-objcopy-hex) $(all-objcopy-bin)
