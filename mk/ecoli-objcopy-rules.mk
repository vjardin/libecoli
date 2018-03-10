# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015, Olivier MATZ <zer0@droids-corp.org>

# dump some infos if debug is enabled
ifeq ($(D),1)
$(call disp_list,------ all-objcopy-hex,$(all-objcopy-hex))
$(foreach objcopy,$(all-objcopy-hex),\
	$(info,out-$(objcopy): $(out-$(objcopy))) \
	$(call disp_list,pre-$(objcopy),$(pre-$(objcopy))) \
)
$(call disp_list,------ all-objcopy-bin,$(all-objcopy-bin))
$(foreach objcopy,$(all-objcopy-bin),\
	$(info,out-$(objcopy): $(out-$(objcopy))) \
	$(call disp_list,pre-$(objcopy),$(pre-$(objcopy))) \
)
endif

# include dependencies and commands files if they exist
$(foreach objcopy,$(all-objcopy-hex) $(all-objcopy-bin),\
	$(eval -include $(call depfile,$(objcopy))) \
	$(eval -include $(call cmdfile,$(objcopy))) \
)

# remove duplicates
filtered-all-objcopy-hex := $(sort $(all-objcopy-hex))

# convert format of executable
$(filtered-all-objcopy-hex): $$(pre-$$@) $$(wildcard $$(dep-$$@)) FORCE
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	@$(call display_deps,$(pre-$(@)),$@,\
		$(call objcopy_hex_cmd,$(pre-$(@)),$@),$?)
	@$(if $(call check_deps,$@,$(call objcopy_hex_cmd,$(pre-$(@)),$@),$?),\
		$(call objcopy_print_cmd,$(pre-$(@)),$@) && \
		$(call objcopy_hex_cmd,$(pre-$(@)),$@) && \
		$(call save_cmd,$(call objcopy_hex_cmd,$(pre-$(@)),$@),$@) && \
		$(call create_empty_depfile,$@))

# remove duplicates
filtered-all-objcopy-bin := $(sort $(all-objcopy-bin))

# convert format of executable
$(filtered-all-objcopy-bin): $$(pre-$$@) $$(wildcard $$(dep-$$@)) FORCE
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	@$(call display_deps,$(pre-$(@)),$@,\
		$(call objcopy_bin_cmd,$(pre-$(@)),$@),$?)
	@$(if $(call check_deps,$@,$(call objcopy_bin_cmd,$(pre-$(@)),$@),$?),\
		$(call objcopy_print_cmd,$(pre-$(@)),$@) && \
		$(call objcopy_bin_cmd,$(pre-$(@)),$@) && \
		$(call save_cmd,$(call objcopy_bin_cmd,$(pre-$(@)),$@),$@) && \
		$(call create_empty_depfile,$@))
