# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015, Olivier MATZ <zer0@droids-corp.org>

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
