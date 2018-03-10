# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015, Olivier MATZ <zer0@droids-corp.org>

# dump some infos if debug is enabled
ifeq ($(D),1)
$(call disp_list,------ all-shlib,$(all-shlib))
$(foreach shlib,$(all-shlib),\
	$(info,out-$(shlib): $(out-$(shlib))) \
	$(call disp_list,pre-$(shlib),$(pre-$(shlib))) \
)
endif

# include dependencies and commands files if they exist
$(foreach shlib,$(all-shlib),\
	$(eval -include $(call depfile,$(shlib))) \
	$(eval -include $(call cmdfile,$(shlib))) \
)

# remove duplicates
filtered-all-shlib := $(sort $(all-shlib))

# link several objects files into one shared object
$(filtered-all-shlib): $$(pre-$$@) $$(wildcard $$(dep-$$@)) FORCE
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	@$(call display_deps,$(pre-$(@)),$@,\
		$(call shlib_cmd,$(pre-$(@)),$@),$?)
	@$(if $(call check_deps,$@,$(call shlib_cmd,$(pre-$(@)),$@),$?),\
		$(call shlib_print_cmd,$(pre-$(@)),$@) && \
		$(call shlib_cmd,$(pre-$(@)),$@) && \
		$(call save_cmd,$(call shlib_cmd,$(pre-$(@)),$@),$@) && \
		$(call create_empty_depfile,$@))
