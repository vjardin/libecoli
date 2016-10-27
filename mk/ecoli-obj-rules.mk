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

# dump some infos if debug is enabled
ifeq ($(D),1)
$(call disp_list,------ all-obj,$(all-obj))
$(foreach obj,$(all-obj),\
	$(info,out-$(obj): $(out-$(obj))) \
	$(call disp_list,pre-$(obj),$(pre-$(obj))) \
)
$(call disp_list,------ all-iobj,$(all-iobj))
$(foreach iobj,$(all-iobj),\
	$(call disp_list,pre-$(iobj),$(pre-$(iobj))) \
)
endif

# if a generated file has the same name than a user target,
# generate an error
conflicts := $(filter $(all-iobj),$(all-targets))
$(if $(conflicts), \
	$(error Intermediate file has the same names than user targets:\
		$(conflicts)))

# include dependencies and commands files if they exist
$(foreach obj,$(all-obj),\
	$(eval -include $(call depfile,$(obj))) \
	$(eval -include $(call cmdfile,$(obj))) \
)
$(foreach iobj,$(all-iobj),\
	$(eval -include $(call depfile,$(iobj))) \
	$(eval -include $(call cmdfile,$(iobj))) \
)

# remove duplicates
filtered-all-iobj := $(sort $(all-iobj))

# convert source files to intermediate object file
$(filtered-all-iobj): $$(pre-$$@) $$(wildcard $$(dep-$$@)) FORCE
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	@$(call display_deps,$(pre-$(@)),$@,$(call compile_cmd,$(pre-$(@)),$@),$?)
	@$(if $(call check_deps,$@,$(call compile_cmd,$(pre-$(@)),$@),$?),\
		$(call compile_print_cmd,$(pre-$(@)),$@) && \
		$(call compile_cmd,$(pre-$(@)),$@) && \
		$(call save_cmd,$(call compile_cmd,$(pre-$(@)),$@),$@) && \
		$(call obj-fixdep,$@))

# remove duplicates
filtered-all-obj := $(sort $(all-obj))

# combine several objects files to one
$(filtered-all-obj): $$(pre-$$@) $$(wildcard $$(dep-$$@)) FORCE
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	@$(call display_deps,$(pre-$(@)),$@,\
		$(call combine_cmd,$(pre-$(@)),$@),$?)
	@$(if $(call check_deps,$@,$(call combine_cmd,$(pre-$(@)),$@),$?),\
		$(call combine_print_cmd,$(pre-$(@)),$@) && \
		$(call combine_cmd,$(pre-$(@)),$@) && \
		$(call save_cmd,$(call combine_cmd,$(pre-$(@)),$@),$@) && \
		$(call create_empty_depfile,$@))
