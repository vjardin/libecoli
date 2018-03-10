# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015, Olivier MATZ <zer0@droids-corp.org>

.PHONY: $(subdir-y)
$(subdir-y): FORCE
	$(Q)$(MAKE) -C $(@) $(mkflags-$(@)) $(MAKECMDGOALS)
