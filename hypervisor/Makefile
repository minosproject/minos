# SPDX-License-Identifier: GPL-2.0
VERSION = 0
PATCHLEVEL = 2
SUBLEVEL = 1
EXTRAVERSION = -rc0
NAME = unstable

PHONY := _all
_all:

MAKEFLAGS += -rR --no-print-directory

ifeq ("$(origin V)", "command line")
  MBUILD_VERBOSE = $(V)
endif
ifndef MBUILD_VERBOSE
  MBUILD_VERBOSE = 0
endif

ifeq ($(MBUILD_VERBOSE),1)
  quiet =
  Q =
else
  quiet=quiet_
  Q = @
endif

ifeq ("$(origin O)", "command line")
	O_LEVEL = $(O)
endif
ifndef O_LEVEL
	O_LEVEL = 2
endif

export quiet Q MBUILD_VERBOSE

srctree 	:= .
objtree		:= .
src		:= $(srctree)
obj		:= $(objtree)

VPATH		:= $(srctree)

export srctree objtree VPATH

version_h := include/config/version.h

clean-targets := %clean
no-dot-config-targets := $(clean-targets) cscope gtags TAGS tags help% $(version_h)
no-sync-config-targets := $(no-dot-config-targets)

config-targets  := 0
mixed-targets   := 0
dot-config      := 1
may-sync-config := 1

ifneq ($(filter $(no-dot-config-targets), $(MAKECMDGOALS)),)
	ifeq ($(filter-out $(no-dot-config-targets), $(MAKECMDGOALS)),)
		dot-config := 0
	endif
endif

ifneq ($(filter $(no-sync-config-targets), $(MAKECMDGOALS)),)
	ifeq ($(filter-out $(no-sync-config-targets), $(MAKECMDGOALS)),)
		may-sync-config := 0
	endif
endif

# For "make -j clean all", "make -j mrproper defconfig all", etc.
ifneq ($(filter $(clean-targets),$(MAKECMDGOALS)),)
        ifneq ($(filter-out $(clean-targets),$(MAKECMDGOALS)),)
                mixed-targets := 1
        endif
endif

ifeq ($(mixed-targets),1)
# ===========================================================================
# We're called with mixed targets (*config and build targets).
# Handle them one by one.

PHONY += $(MAKECMDGOALS) __build_one_by_one

$(filter-out __build_one_by_one, $(MAKECMDGOALS)): __build_one_by_one
	@:

__build_one_by_one:
	$(Q)set -e; \
	for i in $(MAKECMDGOALS); do \
		$(MAKE) -f $(srctree)/Makefile $$i; \
	done

else

include scripts/Minos.config.mk

# Read KERNELRELEASE from include/config/kernel.release (if it exists)
KERNELRELEASE = $(shell cat include/config/kernel.release 2> /dev/null)
KERNELVERSION = $(VERSION)$(if $(PATCHLEVEL),.$(PATCHLEVEL)$(if $(SUBLEVEL),.$(SUBLEVEL)))$(EXTRAVERSION)
export VERSION PATCHLEVEL SUBLEVEL KERNELRELEASE KERNELVERSION

ARCH		?= aarch64
CROSS_COMPILE 	?= aarch64-linux-gnu-

SRCARCH 	:= $(ARCH)

MCONFIG_CONFIG	?= .config
export MCONFIG_CONFIG

# Make variables (CC, etc...)
AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CC) -E
AR		= $(CROSS_COMPILE)ar
NM		= $(CROSS_COMPILE)nm
STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump
LEX		= flex
YACC		= bison
AWK		= awk
PERL		= perl
PYTHON		= python
PYTHON2		= python2
PYTHON3		= python3
CHECK		= sparse
DTC		= dtc

CHECKFLAGS     := -D__minos__ -Dminos -D__STDC__ -Dunix -D__unix__ \
		  -Wbitwise -Wno-return-void -Wno-unknown-attribute $(CF)

# Use LINUXINCLUDE when you must reference the include/ directory.
# Needed to be compatible with the O= option
MINOSINCLUDE    := \
		-I$(srctree)/arch/$(SRCARCH)/include \
		-I$(objtree)/include \
		-I$(srctree)/../include

MBUILD_AFLAGS   := -D__ASSEMBLY__
MBUILD_CFLAGS   := -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs \
		   -fno-strict-aliasing -fno-common -fshort-wchar \
		   -Werror-implicit-function-declaration \
		   -Wno-format-security -O$(O_LEVEL) \
		   -std=gnu89 --static -nostdlib -fno-builtin -g $(MINOSINCLUDE)
MBUILD_CPPFLAGS := -D__KERNEL__
MBUILD_LDFLAGS := --no-undefined

export ARCH SRCARCH CONFIG_SHELL HOSTCC MBUILD_HOSTCFLAGS CROSS_COMPILE AS LD CC DTC
export CPP AR NM STRIP OBJCOPY OBJDUMP MBUILD_HOSTLDFLAGS MBUILD_HOSTLDLIBS
export MAKE LEX YACC AWK GENKSYMS INSTALLKERNEL PERL PYTHON PYTHON2 PYTHON3 UTS_MACHINE

export MBUILD_CPPFLAGS NOSTDINC_FLAGS MINOSINCLUDE OBJCOPYFLAGS MBUILD_LDFLAGS
export MBUILD_CFLAGS CFLAGS_KERNEL CFLAGS_MODULE
export CFLAGS_KASAN CFLAGS_KASAN_NOSANITIZE CFLAGS_UBSAN
export MBUILD_AFLAGS AFLAGS_KERNEL AFLAGS_MODULE
export MBUILD_AFLAGS_KERNEL MBUILD_CFLAGS_KERNEL

export RCS_FIND_IGNORE := \( -name SCCS -o -name BitKeeper -o -name .svn -o    \
			  -name CVS -o -name .pc -o -name .hg -o -name .git \) \
			  -prune -o
export RCS_TAR_IGNORE := --exclude SCCS --exclude BitKeeper --exclude .svn \
			 --exclude CVS --exclude .pc --exclude .hg --exclude .git

PHONY += all
_all: all

core-y		:= core/
drivers-y	:= drivers/ virt/ platform/
external-y	:= external/
libs-y		:=

# The arch Makefile can set ARCH_{CPP,A,C}FLAGS to override the default
# values of the respective MBUILD_* variables
ARCH_CPPFLAGS :=
ARCH_AFLAGS :=
ARCH_CFLAGS :=
-include arch/$(SRCARCH)/Makefile

MBUILD_IMAGE 	:= minos.bin
MBUILD_IMAGE_ELF := minos.elf
MBUILD_IMAGE_SYMBOLS := allsymbols.o

all: include/config/auto.conf $(version_h) minos

minos-dirs	:= $(patsubst %/,%,$(filter %/, $(core-y) $(external-y) $(drivers-y) $(libs-y)))

minos-alldirs	:= $(sort $(minos-dirs) $(patsubst %/,%,$(filter %/, \
			$(core-) $(external-) $(drivers-) $(libs-))))

minos-clean-dirs = $(minos-dirs)
minos-cleandirs = $(minos-alldirs) dtbs/

core-y		:= $(patsubst %/, %/built-in.o, $(core-y))
external-y	:= $(patsubst %/, %/built-in.o, $(external-y))
drivers-y	:= $(patsubst %/, %/built-in.o, $(drivers-y))
libs-y1		:= $(patsubst %/, %/lib.a, $(libs-y))
libs-y2		:= $(patsubst %/, %/built-in.o, $(filter-out %.o, $(libs-y)))

# Externally visible symbols (used by link-minos.sh)
export MBUILD_MINOS_INIT := $(head-y)
export MBUILD_MINOS_MAIN := $(core-y) $(libs-y2) $(drivers-y) $(external-y)
export MBUILD_MINOS_LIBS := $(libs-y1)
export MBUILD_LDS          := $(objtree)/arch/$(SRCARCH)/lds/minos.lds
export LDFLAGS_minos
# used by scripts/package/Makefile
export MBUILD_ALLDIRS := $(sort $(filter-out arch/%,$(minos-alldirs)) arch Documentation include samples scripts tools)

minos-deps := $(MBUILD_LDS) $(MBUILD_MINOS_INIT) $(MBUILD_MINOS_MAIN) $(MBUILD_MINOS_LIBS)

CLEAN_DIRS	:=
clean: rm-dirs 	:= $(CLEAN_DIRS)
clean-dirs      := $(addprefix _clean_, . $(minos-cleandirs))

minos_LDFLAGS := $(MBUILD_LDFLAGS)
minos_LDFLAGS += -T$(MBUILD_LDS) -Map=$(srctree)/linkmap.txt

PHONY += $(clean-dirs) clean distclean
$(clean-dirs):
	$(Q) $(MAKE) $(clean)=$(patsubst _clean_%,%,$@)

minos: $(minos-deps) tools/generate_allsymbols.py
	$(Q) echo "  LD      .tmp.minos.elf"
	$(Q) $(LD) $(minos_LDFLAGS) -o .tmp.minos.elf $(MBUILD_MINOS_INIT) $(MBUILD_MINOS_MAIN) $(MBUILD_MINOS_LIBS)
	$(Q) echo "  NM      .tmp.minos.symbols"
	$(Q) $(NM) -n .tmp.minos.elf > .tmp.minos.symbols
	$(Q) echo "  PYTHON  allsymbols.S"
	$(Q) python3 tools/generate_allsymbols.py .tmp.minos.symbols allsymbols.S
	$(Q) echo "  CC      $(MBUILD_IMAGE_SYMBOLS)"
	$(Q) $(CC) $(CCFLAG) $(MBUILD_CFLAGS) -c allsymbols.S -o $(MBUILD_IMAGE_SYMBOLS)
	$(Q) echo "  LD      $(MBUILD_IMAGE_ELF)"
	$(Q) $(LD) $(minos_LDFLAGS) -o $(MBUILD_IMAGE_ELF) $(MBUILD_MINOS_INIT) $(MBUILD_MINOS_MAIN) $(MBUILD_MINOS_LIBS) $(MBUILD_IMAGE_SYMBOLS)
	$(Q) echo "  OBJCOPY $(MBUILD_IMAGE)"
	$(Q) $(OBJCOPY) -O binary $(MBUILD_IMAGE_ELF) $(MBUILD_IMAGE)
	$(Q) echo "  OBJDUMP minos.s"
	$(Q) $(OBJDUMP) $(MBUILD_IMAGE_ELF) -D > minos.s

# The actual objects are generated when descending,
# make sure no implicit rule kicks in
$(sort $(minos-deps)): $(minos-dirs) ;

# here goto each directory to generate built-in.o
PHONY += $(minos-dirs)
$(minos-dirs):
	$(Q)$(MAKE) $(build)=$@

define filechk_version.h
	(echo \#define MINOS_VERSION_CODE $(shell                         \
	expr $(VERSION) \* 65536 + 0$(PATCHLEVEL) \* 256 + 0$(SUBLEVEL)) > $@; \
	echo '#define MINOS_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))' >> $@; \
	echo '#define MINOS_VERSION_STR "v$(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION) $(NAME)"' >> $@;)
endef

$(version_h) : Makefile
	$(Q) mkdir -p include/config
	$(Q) $(call filechk_version.h)

PHONY += include/config/auto.conf
include/config/auto.conf:
	$(Q)test -e include/config/config.h -a -e $@ || (		\
	echo >&2;							\
	echo >&2 "  ERROR: Minos configuration is invalid.";		\
	echo >&2 "         include/config/autoconf.h or $@ are missing.";\
	echo >&2 "         Run 'make xxx_defconfig' on minos src to fix it.";	\
	echo >&2 ;							\
	/bin/false)

dtbs: FORCE
	$(Q)$(MAKE) $(build)=$@

clean: $(clean-dirs)
	$(Q) echo "  CLEAN   all .o .*.d *.dtb built-in.o"
	$(Q) echo "  CLEAN   allsymbols.o allsymbols.S linkmap.txt minos.s .tmp.minos.elf .tmp.minos.symbols minos.bin minos.elf"
	$(Q) rm -f allsymbols.o allsymbols.S linkmap.txt minos.s .tmp.minos.elf .tmp.minos.symbols minos.bin minos.elf

distclean: clean
	$(Q) echo "  CLEAN   .config include/config"
	$(Q) rm -rf include/config .config
	$(Q) echo "  CLEAN   tags cscope.in.out cscope.out cscope.po.out"
	$(Q) rm -f tags cscope.in.out cscope.out cscope.po.out

%defconfig:
	$(Q)test -e configs/$@ || (		\
	echo >&2;			\
	echo >&2 "  ERROR: $@ doest exist.";		\
	echo >&2 ;							\
	/bin/false)
	$(Q) echo "  GEN      .config configs/$@"
	$(Q) mkdir -p include/config
	$(Q) cp configs/$@ $(srctree)/.config
# Generate include/config/config.h and include/config/auto.conf
	$(Q) echo "  PYTHON  include/config/config.h"
	$(Q) echo "  PYTHON  include/config/auto.conf"
	$(Q) python $(srctree)/tools/generate_mvconfig.py $(srctree)/.config $(srctree)/include/config

endif

PHONY += FORCE
FORCE:

# Declare the contents of the PHONY variable as phony.  We keep that
# information in a variable so we can use it in if_changed and friends.
.PHONY: $(PHONY)
