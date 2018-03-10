# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015, Olivier MATZ <zer0@droids-corp.org>

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
