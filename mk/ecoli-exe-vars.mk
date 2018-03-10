# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015, Olivier MATZ <zer0@droids-corp.org>

# exe-y-$(exe) is provided by the user
#   $(exe) is the path of the binary, and the variable contains
#   the list of sources. Several exe-y-$(exe) can be present.

# list all exe builds requested by user
all-exe := $(patsubst exe-y-%,%,$(filter exe-y-%,$(.VARIABLES)))

# add them to the list of targets
all-targets += $(all-exe)

# for each exe, create the following variables:
#   out-$(exe) = output path of the executable
#   pre-$(exe) = list of prerequisites for this executable
# Some source files need intermediate objects, we define these variables
# for them too, and add them in a list: $(all-iobj).
# Last, we add the generated files in $(all-clean-file).
$(foreach exe,$(all-exe),\
	$(eval out-$(exe) := $(dir $(exe))) \
	$(eval pre-$(exe) := ) \
	$(foreach src,$(exe-y-$(exe)), \
		$(if $(call is_cc_source,$(src)), \
			$(eval iobj := $(call src2iobj,$(src),$(out-$(exe)))) \
			$(eval pre-$(iobj) := $(src)) \
			$(eval all-iobj += $(iobj)) \
			$(eval all-clean-file += $(iobj)) \
			$(eval pre-$(exe) += $(iobj)) \
		, \
		$(if $(call is_obj_source,$(src)),\
			$(eval pre-$(exe) += $(src)) \
		, \
		$(if $(call is_alib_source,$(src)),\
			$(eval pre-$(exe) += $(src)) \
		, \
		$(error "unsupported source format: $(src)")))) \
	)\
	$(eval all-clean-file += $(exe)) \
)

# link several *.o files into a exeary
#   $1: sources (*.o) (*.a)
#   $2: dst (xyz.o too)
link_cmd = $(CC) $(LDFLAGS) $(ldflags-$(2)) -o $(2) $(filter %.o,$(1)) \
	 $(filter %.a,$(1)) $(LDLIBS) $(ldlibs-$(2))

# print line used to link object files
ifeq ($(V),1)
link_print_cmd = echo $(call protect_quote,$(call link_cmd,$1,$2))
else
link_print_cmd = echo "  EXE $(2)"
endif

all-clean-file += $(all-exe)
