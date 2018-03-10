# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015, Olivier MATZ <zer0@droids-corp.org>

# dump some infos if debug is enabled
ifeq ($(D),1)
$(call disp_list,------ all-copy,$(all-copy))
$(foreach copy,$(all-copy),\
	$(info,out-$(copy): $(out-$(copy))) \
	$(call disp_list,pre-$(copy),$(pre-$(copy))) \
)
endif

# include dependencies and commands files if they exist
$(foreach copy,$(all-copy),\
	$(eval -include $(call depfile,$(copy))) \
	$(eval -include $(call cmdfile,$(copy))) \
)

# remove duplicates
filtered-all-copy := $(sort $(all-copy))

# convert format of executable
$(filtered-all-copy): $$(pre-$$@) $$(wildcard $$(dep-$$@)) FORCE
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	@$(call display_deps,$(pre-$(@)),$@,\
		$(call copy_cmd,$(pre-$(@)),$@),$?)
	@$(if $(call check_deps,$@,$(call copy_cmd,$(pre-$(@)),$@),$?),\
		$(call copy_print_cmd,$(pre-$(@)),$@) && \
		$(call copy_cmd,$(pre-$(@)),$@) && \
		$(call save_cmd,$(call copy_cmd,$(pre-$(@)),$@),$@) && \
		$(call create_empty_depfile,$@))
