# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015, Olivier MATZ <zer0@droids-corp.org>

# dump some infos if debug is enabled
ifeq ($(D),1)
$(call disp_list,------ all-exe,$(all-exe))
$(foreach exe,$(all-exe),\
	$(info,out-$(exe): $(out-$(exe))) \
	$(call disp_list,pre-$(exe),$(pre-$(exe))) \
)
endif

# include dependencies and commands files if they exist
$(foreach exe,$(all-exe),\
	$(eval -include $(call depfile,$(exe))) \
	$(eval -include $(call cmdfile,$(exe))) \
)

# remove duplicates
filtered-all-exe := $(sort $(all-exe))

# link several objects files into one executable
$(filtered-all-exe): $$(pre-$$@) $$(wildcard $$(dep-$$@)) FORCE
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	@$(call display_deps,$(pre-$(@)),$@,\
		$(call link_cmd,$(pre-$(@)),$@),$?)
	@$(if $(call check_deps,$@,$(call link_cmd,$(pre-$(@)),$@),$?),\
		$(call link_print_cmd,$(pre-$(@)),$@) && \
		$(call link_cmd,$(pre-$(@)),$@) && \
		$(call save_cmd,$(call link_cmd,$(pre-$(@)),$@),$@) && \
		$(call create_empty_depfile,$@))
