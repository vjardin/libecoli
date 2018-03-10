# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015, Olivier MATZ <zer0@droids-corp.org>

# remove files
#  $1: files
clean_cmd = rm -rf $(1)

# print line used to clean files
ifeq ($(V),1)
clean_print_cmd = echo $(call protect_quote,$(call clean_cmd,$1))
else
clean_print_cmd = echo "  CLEAN $(CURDIR)"
endif
