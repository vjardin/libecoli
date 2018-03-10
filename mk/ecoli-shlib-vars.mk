# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015, Olivier MATZ <zer0@droids-corp.org>

# shlib-y-$(shlib) is provided by the user
#   $(shlib) is the path of the shared library, and the variable
#   contains the list of sources. Several shlib-y-$(shlib) can be
#   present.

# list all shlib builds requested by user
all-shlib := $(patsubst shlib-y-%,%,$(filter shlib-y-%,$(.VARIABLES)))

# add them to the list of targets
all-targets += $(all-shlib)

# for each shlib, create the following variables:
#   out-$(shlib) = output path of the shlibcutable
#   pre-$(shlib) = list of prerequisites for this shlibcutable
# Some source files need intermediate objects, we define these variables
# for them too, and add them in a list: $(all-iobj).
# Last, we add the generated files in $(all-clean-file).
$(foreach shlib,$(all-shlib),\
	$(eval out-$(shlib) := $(dir $(shlib))) \
	$(eval pre-$(shlib) := ) \
	$(foreach src,$(shlib-y-$(shlib)), \
		$(if $(call is_cc_source,$(src)), \
			$(eval iobj := $(call src2iobj,$(src),$(out-$(shlib)))) \
			$(eval pre-$(iobj) := $(src)) \
			$(eval all-iobj += $(iobj)) \
			$(eval all-clean-file += $(iobj)) \
			$(eval pre-$(shlib) += $(iobj)) \
		, \
		$(if $(call is_obj_source,$(src)),\
			$(eval pre-$(shlib) += $(src)) \
		, \
		$(error "unsupported source format: $(src)"))) \
	)\
	$(eval all-clean-file += $(shlib)) \
)

# link several *.o files into a shared libary
#   $1: sources (*.o)
#   $2: dst (xyz.so)
shlib_cmd = $(CC) $(LDFLAGS) $(ldflags-$(2)) -shared -o $(2) $(1)

# print line used to shlib object files
ifeq ($(V),1)
shlib_print_cmd = echo $(call protect_quote,$(call shlib_cmd,$1,$2))
else
shlib_print_cmd = echo "  SHLIB $(2)"
endif

all-clean-file += $(all-shlib)
