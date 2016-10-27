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

# ar-y-$(ar) is provided by the user
#   $(ar) is the path of the static library, and the variable contains
#   the list of sources. Several ar-y-$(ar) can be present.

# list all ar builds requested by user
all-ar := $(patsubst ar-y-%,%,$(filter ar-y-%,$(.VARIABLES)))

# add them to the list of targets
all-targets += $(all-ar)

# for each ar, create the following variables:
#   out-$(ar) = output path of the arcutable
#   pre-$(ar) = list of prerequisites for this arcutable
# Some source files need intermediate objects, we define these variables
# for them too, and add them in a list: $(all-iobj).
# Last, we add the generated files in $(all-clean-file).
$(foreach ar,$(all-ar),\
	$(eval out-$(ar) := $(dir $(ar))) \
	$(eval pre-$(ar) := ) \
	$(foreach src,$(ar-y-$(ar)), \
		$(if $(call is_cc_source,$(src)), \
			$(eval iobj := $(call src2iobj,$(src),$(out-$(ar)))) \
			$(eval pre-$(iobj) := $(src)) \
			$(eval all-iobj += $(iobj)) \
			$(eval all-clean-file += $(iobj)) \
			$(eval pre-$(ar) += $(iobj)) \
		, \
		$(if $(call is_obj_source,$(src)),\
			$(eval pre-$(ar) += $(src)) \
		, \
		$(error "unsupported source format: $(src)"))) \
	)\
	$(eval all-clean-file += $(ar)) \
)

# link several *.o files into a static libary
#   $1: sources (*.o)
#   $2: dst (xyz.a)
ar_cmd = ar crsD $(2) $(1)

# print line used to ar object files
ifeq ($(V),1)
ar_print_cmd = echo $(call protect_quote,$(call ar_cmd,$1,$2))
else
ar_print_cmd = echo "  AR $(2)"
endif

all-clean-file += $(all-ar)
