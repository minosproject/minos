# SPDX-License-Identifier: GPL-2.0
# ==========================================================================
# Cleaning up
# ==========================================================================

src := $(obj)

PHONY := __clean
__clean:

include scripts/Minos.config.mk

# The filename Kbuild has precedence over Makefile
kbuild-dir := $(if $(filter /%,$(src)),$(src),$(srctree)/$(src))
include $(if $(wildcard $(kbuild-dir)/Kbuild), $(kbuild-dir)/Kbuild, $(kbuild-dir)/Makefile)

# Figure out what we need to build from the various variables
# ==========================================================================

__subdir-y	:= $(patsubst %/,%,$(filter %/, $(obj-y)))
subdir-y	+= $(__subdir-y)
__subdir-	:= $(patsubst %/,%,$(filter %/, $(obj-)))
subdir-		+= $(__subdir-)

# Subdirectories we need to descend into

subdir-ym	:= $(sort $(subdir-y))
subdir-ymn      := $(sort $(subdir-ym) $(subdir-))

# remove the directly
#real-obj-y	= $(filter-out %/, $(obj-y))
#real-dep-y	:= $(addsuffix .d, $(real-obj-y))
#real-dep-y	:= $(addprefix $(obj)/., $(real-dep-y))

# Add subdir path

subdir-ymn	:= $(addprefix $(obj)/,$(subdir-ymn))

# build a list of files to remove, usually relative to the current
# directory

#__clean-files	:= $(addprefix $(obj)/, $(real-obj-y))
__clean-files	+= $(obj)/built-in.o
__clean-files	+= $(obj)/.built-in.o.d
#__clean-files	+= $(real-dep-y)
__clean-files	+= $(wildcard $(obj)/*.o)
__clean-files	+= $(wildcard $(obj)/.*.d)
__clean-files	+= $(wildcard $(obj)/*.lds)
__clean-files	+= $(wildcard $(obj)/*.dtb)

__clean-dirs    :=

# ==========================================================================
__clean: $(subdir-ymn)
	$(Q) rm -f $(__clean-files)
	$(Q) rm -rf $(__clean-dirs)

# ===========================================================================
# Generic stuff
# ===========================================================================

# Descending
# ---------------------------------------------------------------------------

PHONY += $(subdir-ymn)
$(subdir-ymn):
	$(Q)$(MAKE) $(clean)=$@

.PHONY: $(PHONY)
