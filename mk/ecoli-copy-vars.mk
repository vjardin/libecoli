# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015, Olivier MATZ <zer0@droids-corp.org>

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
