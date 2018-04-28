ARCH 		:= aarch64
CROSS_COMPILE 	:= aarch64-elf-
CC 		:= $(CROSS_COMPILE)gcc
LD 		:= $(CROSS_COMPILE)ld
OBJ_COPY	:= $(CROSS_COMPILE)objcopy
OBJ_DUMP 	:= $(CROSS_COMPILE)objdump
QUIET ?= @

#PLATFORM	:= fvp
#BOARD		:= armv8-fvp

INCLUDE_DIR 	:= include/mvisor/*.h include/asm/*.h include/config/*.h include/drivers/*.h
#INCLUDE_DIR 	:=

CCFLAG 		:= --static -nostdlib -fno-builtin -g -march=armv8-a -I$(PWD)/include
LDS 		:= arch/$(ARCH)/lds/mvisor.ld.c

OUT 		:= out
OUT_CORE 	= $(OUT)/mvisor
OUT_ARCH 	= $(OUT)/$(ARCH)
OUT_MVISOR_VMS	= $(OUT)/mvisor_vms
OUT_DRIVERS	= $(OUT)/drivers
TARGET_LDS	= $(OUT)/mvisor.lds

mvisor_elf 	:= $(OUT)/mvisor.elf
mvisor_bin 	:= $(OUT)/mvisor.bin
mvisor_dump 	:= $(OUT)/mvisor.s

SRC_ARCH_C	:= $(wildcard arch/$(ARCH)/*.c)
SRC_ARCH_S	:= $(wildcard arch/$(ARCH)/*.S)
SRC_CORE	:= $(wildcard mvisor/*.c)
SRC_MVISOR_VMS	:= $(wildcard mvisor_vms/*.c)
SRC_DRIVERS	:= $(wildcard drivers/*.c)

VPATH		:= mvisor:arch/$(ARCH):mvisor_vms:drivers

LDFLAG 		:= -T$(TARGET_LDS) -Map=$(OUT)/linkmap.txt

.SUFFIXES:
.SUFFIXES: .S .c

ifeq ($(QUIET),@)
PROGRESS = @echo Compiling $< ...
endif

_OBJ_ARCH	+= $(addprefix $(OUT_ARCH)/, $(patsubst %.c,%.o, $(notdir $(SRC_ARCH_C))))
_OBJ_ARCH	+= $(addprefix $(OUT_ARCH)/, $(patsubst %.S,%.o, $(notdir $(SRC_ARCH_S))))
OBJ_ARCH	= $(subst out/$(ARCH)/boot.o,,$(_OBJ_ARCH))
OBJ_CORE	+= $(addprefix $(OUT_CORE)/, $(patsubst %.c,%.o, $(notdir $(SRC_CORE))))
OBJ_MVISOR_VMS	+= $(addprefix $(OUT_MVISOR_VMS)/, $(patsubst %.c,%.o, $(notdir $(SRC_MVISOR_VMS))))
OBJ_DRIVERS	+= $(addprefix $(OUT_DRIVERS)/, $(patsubst %.c,%.o, $(notdir $(SRC_DRIVERS))))

OBJECT		= $(OUT_ARCH)/boot.o $(OBJ_ARCH) $(OBJ_CORE) $(OBJ_MVISOR_VMS) $(OBJ_DRIVERS)

all: $(OUT) $(OUT_CORE) $(OUT_ARCH) $(OUT_DRIVERS)  $(OUT_MVISOR_VMS) $(mvisor_bin)

$(mvisor_bin) : $(mvisor_elf)
	$(QUIET) $(OBJ_COPY) -O binary $(mvisor_elf) $(mvisor_bin)
	$(QUIET) $(OBJ_DUMP) $(mvisor_elf) -D > $(mvisor_dump)

$(mvisor_elf) : $(OBJECT) $(TARGET_LDS)
	@echo Linking $@
	$(QUIET) $(LD) $(LDFLAG) -o $(mvisor_elf) $(OBJECT) $(LDPATH)
	@echo Done.

$(TARGET_LDS) : $(LDS) $(INCLUDE_DIR)
	@echo Generate LDS file
	$(QUIET) $(CC) $(CCFLAG) -E -P $(LDS) -o $(TARGET_LDS)

$(OUT) $(OUT_CORE) $(OUT_ARCH) $(OUT_MVISOR_VMS) $(OUT_DRIVERS):
	@ mkdir -p $@

$(OUT_ARCH)/%.o: %.c $(INCLUDE_DIR)
	$(PROGRESS)
	$(QUIET) $(CC) $(CCFLAG) -c $< -o $@

$(OUT_ARCH)/boot.o: arch/$(ARCH)/boot.S $(INCLUDE_DIR)
	$(PROGRESS)
	$(QUIET) $(CC) $(CCFLAG) -c arch/$(ARCH)/boot.S -o $@

$(OUT_ARCH)/%.o: %.S $(INCLUDE_DIR) 
	$(PROGRESS)
	$(QUIET) $(CC) $(CCFLAG) -c $< -o $@

$(OUT_CORE)/%.o: %.c $(INCLUDE_DIR)
	$(PROGRESS)
	$(QUIET) $(CC) $(CCFLAG) -c $< -o $@

$(OUT_MVISOR_VMS)/%.o: %.c $(INCLUDE_DIR)
	$(PROGRESS)
	$(QUIET) $(CC) $(CCFLAG) -c $< -o $@

$(OUT_DRIVERS)/%.o: %.c $(INCLUDE_DIR)
	$(PROGRESS)
	$(QUIET) $(CC) $(CCFLAG) -c $< -o $@

.PHONY: clean run app

clean:
	@ rm -rf out
	@ echo "All build has been cleaned"
