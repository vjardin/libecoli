# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015, Olivier MATZ <zer0@droids-corp.org>

# subdir-y is provided by the user
#  it contains the list of directory to build

# add them to the list of targets
all-targets += $(subdir-y)
all-clean-target += $(subdir-y)
