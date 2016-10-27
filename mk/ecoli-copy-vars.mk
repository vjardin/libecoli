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

# copy a file
# copy-y-$(copy) is provided by the user
#   $(copy) is the path of the directory containing the destination
#   files, and the variable contains the path of the files to copy. Several
#   copy-y-$(copy) can be present.

# list all path requested by user
_all-copy := $(patsubst copy-y-%,%,$(filter copy-y-%,$(.VARIABLES)))
all-copy :=

# for each copy, create the following variables:
#   out-$(copy) = output path of the executable
#   pre-$(copy) = list of prerequisites for this executable
# We also add the files in $(all-copy).
$(foreach copy,$(_all-copy),\
	$(if $(notdir $(copy)), \
		$(if $(call compare,$(words $(copy-y-$(copy))),1), \
			$(error "only one source file is allowed in copy-y-$(copy)")) \
		$(eval dst := $(dir $(copy))$(notdir $(copy-y-$(copy)))) \
		$(eval out-$(copy) := $(dir $(copy))) \
		$(eval pre-$(copy) := $(copy-y-$(copy))) \
		$(eval all-copy += $(dst)) \
	, \
		$(foreach src,$(copy-y-$(copy)),\
			$(eval dst := $(copy)$(notdir $(src))) \
			$(eval out-$(copy) := $(copy)) \
			$(eval pre-$(dst) := $(src)) \
			$(eval all-copy += $(dst)) \
		) \
	) \
)

# add them to the list of targets and clean
all-targets += $(all-copy)
all-clean-file += $(all-copy)

# convert format of executable from elf to ihex
#   $1: source executable (elf)
#   $2: destination file
copy_cmd = $(CP) $(1) $(2)

# print line used to convert executable format
ifeq ($(V),1)
copy_print_cmd = echo $(call protect_quote,$(call copy_cmd,$1,$2))
else
copy_print_cmd = echo "  COPY $(2)"
endif
