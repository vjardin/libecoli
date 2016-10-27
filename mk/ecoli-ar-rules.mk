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
