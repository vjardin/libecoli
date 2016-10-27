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

empty:=
space:= $(empty) $(empty)
indent:= $(space)$(space)

# define a newline char, useful for debugging with $(info)
define newline


endef

# $(prefix shell commands with $(Q) to silent them, except if V=1
Q=@
ifeq ("$(V)-$(origin V)", "1-command line")
Q=
endif

# set variable $1 to $2 if the variable has an implicit value or
# is not defined
#  $1 variable name
#  $2 new variable content
set_default = $(if \
	$(call not,$(or \
		$(compare $(origin $(1)),default), \
		$(compare $(origin $(1)),undefined) \
	)),\
	$(eval $(1) = $(2)) \
)

# display a list
#  $1 title
#  $2 list
disp_list = $(info $(1)$(newline)\
	$(addsuffix $(newline),$(addprefix $(space),$(2))))

# add a dot in front of the file name
#  $1 list of paths
#  return: full paths with files prefixed by a dot
dotfile = $(strip $(foreach f,$(1),\
	$(join $(dir $f),.$(notdir $f))))

# convert source/obj files into dot-dep filename
#  $1 list of paths
#  return: full paths with files prefixed by a dot and suffixed with .d
depfile = $(strip $(call dotfile,$(addsuffix .d,$(1))))

# convert source/obj files into dot-dep filename
#  $1 list of paths
#  return: full paths with files prefixed by a dot and suffixed with .d.tmp
file2tmpdep = $(strip $(call dotfile,$(addsuffix .d.tmp,$(1))))

# convert source/obj files into dot-cmd filename
#  $1 list of paths
#  return: full paths with files prefixed by a dot and suffixed with .cmd
cmdfile = $(strip $(call dotfile,$(addsuffix .cmd,$(1))))

# add a \ before each quote
protect_quote = $(subst ','\'',$(1))
#'# editor syntax highlight fix

# return an non-empty string if $1 is empty, and vice versa
#  $1 a string
not = $(if $1,,true)

# return 1 if parameter is a non-empty string, else 0
boolean = $(if $1,1,0)

# return an empty string if string are equal
compare = $(strip $(subst $(1),,$(2)) $(subst $(2),,$(1)))

# return a non-empty string if a file does not exist
#  $1: file
file_missing = $(call compare,$(wildcard $1),$1)

# return a non-empty string if cmdline changed
#  $1: file to be built
#  $2: the command to build it
cmdline_changed = $(call compare,$(strip $(cmd-$(1))),$(strip $(2)))

# return an non-empty string if the .d file does not exist
#  $1: the dep file (.d)
depfile_missing = $(call compare,$(wildcard $(1)),$(1))

# return a non-empty string if, according to dep-xyz variable, a file
# needed to build $1 does not exist. In this case we need to rebuild
# the file and the .d file.
#  $1: file to be built
dep-missing = $(call compare,$(wildcard $(dep-$(1))),$(dep-$(1)))

# return an empty string if no prereq is newer than target
#  $1: list of prerequisites newer than target ($?)
dep-newer = $(strip $(filter-out FORCE,$(1)))

# display why a file should be re-built
#  $1: source files
#  $2: dst file
#  $3: build command
#  $4: all prerequisites newer than target ($?)
ifeq ($(D),1)
display_deps = \
	echo -n "$1 -> $2 " ; \
	echo -n "file_missing=$(call boolean,$(call file_missing,$(2))) " ; \
	echo -n "cmdline_changed=$(call boolean,$(call cmdline_changed,$(2),$(3))) " ; \
	echo -n "depfile_missing=$(call boolean,$(call depfile_missing,$(call depfile,$(2)))) " ; \
	echo -n "dep-missing=$(call boolean,$(call dep-missing,$(2))) " ; \
	echo "dep-newer=$(call boolean,$(call dep-newer,$(4)))"
else
display_deps=
endif

# return an empty string if a file should be rebuilt
#  $1: dst file
#  $2: build command
#  $3: all prerequisites newer than target ($?)
check_deps = \
	$(or $(call file_missing,$(1)),\
	$(call cmdline_changed,$(1),$(2)),\
	$(call depfile_missing,$(call depfile,$(1))),\
	$(call dep-missing,$(1)),\
	$(call dep-newer,$(3)))

# create a depfile (.d) with no additional deps
#  $1: object file (.o)
create_empty_depfile = echo "dep-$(1) =" > $(call depfile,$(1))

# save a command in a file
#  $1: command to build the file
#  $2: name of the file
save_cmd = echo "cmd-$(2) = $(call protect_quote,$(1))" > $(call cmdfile,$(2))

# remove the FORCE target from the list of all prerequisites $+
#  no arguments, use $+
prereq = $(filter-out FORCE,$(+))
