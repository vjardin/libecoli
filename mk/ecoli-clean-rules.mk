# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015, Olivier MATZ <zer0@droids-corp.org>

.PHONY: _ecoli_clean
_ecoli_clean: $(all-clean-target) FORCE
	@$(call clean_print_cmd,$(all-clean-file) $(call depfile,$(all-clean-file)) \
		$(call cmdfile,$(all-clean-file))) && \
	$(call clean_cmd,$(all-clean-file) $(call depfile,$(all-clean-file)) \
		$(call cmdfile,$(all-clean-file)))
