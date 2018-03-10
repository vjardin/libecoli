# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015, Olivier MATZ <zer0@droids-corp.org>

# ---- variables that must be defined:
#
#  ECOLI: path to ecoli root
#

ifeq ($(ECOLI),)
$(error ECOLI environment variable is not defined)
endif

MAKEFLAGS += --no-print-directory

include $(ECOLI)/mk/ecoli-tools.mk

include $(ECOLI)/mk/ecoli-vars.mk

