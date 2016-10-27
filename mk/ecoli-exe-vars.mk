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
