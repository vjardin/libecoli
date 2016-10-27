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

# create a symbolic link of a file
# slink-y-$(slink) is provided by the user
#   $(slink) is the path of the directory containing the destination
#   files, and the variable contains the path of the files to linked. Several
#   slink-y-$(slink) can be present.

# list all path requested by user
_all-slink := $(patsubst slink-y-%,%,$(filter slink-y-%,$(.VARIABLES)))
all-slink :=

# for each slink, create the following variables:
#   out-$(slink) = output path of the executable
#   pre-$(slink) = list of prerequisites for this executable
# We also add the files in $(all-slink).
$(foreach slink,$(_all-slink),\
	$(if $(notdir $(slink)), \
		$(if $(call compare,$(words $(slink-y-$(slink))),1), \
			$(error "only one source file is allowed in slink-y-$(slink)")) \
		$(eval dst := $(dir $(slink))$(notdir $(slink-y-$(slink)))) \
		$(eval out-$(slink) := $(dir $(slink))) \
		$(eval pre-$(slink) := $(slink-y-$(slink))) \
		$(eval all-slink += $(dst)) \
	, \
		$(foreach src,$(slink-y-$(slink)),\
			$(eval dst := $(slink)$(notdir $(src))) \
			$(eval out-$(slink) := $(slink)) \
			$(eval pre-$(dst) := $(src)) \
			$(eval all-slink += $(dst)) \
		) \
	) \
)

# add them to the list of targets and clean
all-targets += $(all-slink)
all-clean-file += $(all-slink)

# convert format of executable from elf to ihex
#   $1: source executable (elf)
#   $2: destination file
slink_cmd = $(LN) -nsf $(abspath $(1)) $(2)

# print line used to convert executable format
ifeq ($(V),1)
slink_print_cmd = echo $(call protect_quote,$(call slink_cmd,$1,$2))
else
slink_print_cmd = echo "  SLINK $(2)"
endif
