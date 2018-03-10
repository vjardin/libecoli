# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015, Olivier MATZ <zer0@droids-corp.org>

# dump some infos if debug is enabled
ifeq ($(D),1)
$(call disp_list,------ all-ar,$(all-ar))
$(foreach ar,$(all-ar),\
	$(info,out-$(ar): $(out-$(ar))) \
	$(call disp_list,pre-$(ar),$(pre-$(ar))) \
)
endif

# include dependencies and commands files if they exist
$(foreach ar,$(all-ar),\
	$(eval -include $(call depfile,$(ar))) \
	$(eval -include $(call cmdfile,$(ar))) \
)

# remove duplicates
filtered-all-ar := $(sort $(all-ar))

# link several objects files into one shared object
$(filtered-all-ar): $$(pre-$$@) $$(wildcard $$(dep-$$@)) FORCE
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	@$(call display_deps,$(pre-$(@)),$@,\
		$(call ar_cmd,$(pre-$(@)),$@),$?)
	@$(if $(call check_deps,$@,$(call ar_cmd,$(pre-$(@)),$@),$?),\
		$(call ar_print_cmd,$(pre-$(@)),$@) && \
		$(call ar_cmd,$(pre-$(@)),$@) && \
		$(call save_cmd,$(call ar_cmd,$(pre-$(@)),$@),$@) && \
		$(call create_empty_depfile,$@))
