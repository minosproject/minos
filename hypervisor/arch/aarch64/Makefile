#
# arch/arm64/Makefile
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies.
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1995-2001 by Russell King
#
ifeq ($(CONFIG_ARM64_LSE_ATOMICS), y)
  ifeq ($(lseinstr),)
$(warning LSE atomics not supported by binutils)
  endif
endif

ifeq ($(CONFIG_ARCH_AARCH64), y)
brokengasinst := $(call as-instr,1:\n.inst 0\n.rept . - 1b\n\nnop\n.endr\n,,-DCONFIG_BROKEN_GAS_INST=1)

  ifneq ($(brokengasinst),)
$(warning Detected assembler with broken .inst; disassembly will be unreliable)
  endif
endif

# Check for binutils support for specific extensions
lseinstr := $(call as-instr,.arch_extension lse,-DCONFIG_AS_LSE=1)

MBUILD_LDFLAGS	+= -X

MBUILD_CFLAGS	+= -mgeneral-regs-only $(lseinstr) $(brokengasinst)
MBUILD_CFLAGS	+= -fno-asynchronous-unwind-tables -march=armv8-a
MBUILD_AFLAGS	+= $(lseinstr) $(brokengasinst)

ifeq ($(CONFIG_CPU_BIG_ENDIAN), y)
MBUILD_CFLAGS	+= -mbig-endian
# Prefer the baremetal ELF build target, but not all toolchains include
# it so fall back to the standard linux version if needed.
MBUILD_LDFLAGS	+= -EB
else
MBUILD_CFLAGS	+= -mlittle-endian
MBUILD_LDFLAGS	+= -EL
endif

# Default value
head-y		:= arch/aarch64/core/boot.o

core-y		+= arch/aarch64/core/
core-y		+= arch/aarch64/lib/
core-y		+= arch/aarch64/lds/
