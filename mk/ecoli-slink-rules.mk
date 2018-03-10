# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015, Olivier MATZ <zer0@droids-corp.org>

# dump some infos if debug is enabled
ifeq ($(D),1)
$(call disp_list,------ all-slink,$(all-slink))
$(foreach slink,$(all-slink),\
	$(info,out-$(slink): $(out-$(slink))) \
	$(call disp_list,pre-$(slink),$(pre-$(slink))) \
)
endif

# include dependencies and commands files if they exist
$(foreach slink,$(all-slink),\
	$(eval -include $(call depfile,$(slink))) \
	$(eval -include $(call cmdfile,$(slink))) \
)

# remove duplicates
filtered-all-slink := $(sort $(all-slink))

# convert format of executable
$(filtered-all-slink): $$(pre-$$@) $$(wildcard $$(dep-$$@)) FORCE
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	@$(call display_deps,$(pre-$(@)),$@,\
		$(call slink_cmd,$(pre-$(@)),$@),$?)
	@$(if $(call check_deps,$@,$(call slink_cmd,$(pre-$(@)),$@),$?),\
		$(call slink_print_cmd,$(pre-$(@)),$@) && \
		$(call slink_cmd,$(pre-$(@)),$@) && \
		$(call save_cmd,$(call slink_cmd,$(pre-$(@)),$@),$@) && \
		$(call create_empty_depfile,$@))
