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
LDS 		:= arch/$(ARCH)/lds/vmm.ld.c

OUT 		:= out
OUT_CORE 	= $(OUT)/mvisor
OUT_ARCH 	= $(OUT)/$(ARCH)
OUT_VMM_VMS	= $(OUT)/vmm_vms
OUT_DRIVERS	= $(OUT)/drivers
TARGET_LDS	= $(OUT)/vmm.lds

vmm_elf 	:= $(OUT)/vmm.elf
vmm_bin 	:= $(OUT)/vmm.bin
vmm_dump 	:= $(OUT)/vmm.s

SRC_ARCH_C	:= $(wildcard arch/$(ARCH)/*.c)
SRC_ARCH_S	:= $(wildcard arch/$(ARCH)/*.S)
SRC_CORE	:= $(wildcard mvisor/*.c)
SRC_VMM_VMS	:= $(wildcard vmm_vms/*.c)
SRC_DRIVERS	:= $(wildcard drivers/*.c)

VPATH		:= mvisor:arch/$(ARCH):vmm_vms:drivers

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
OBJ_VMM_VMS	+= $(addprefix $(OUT_VMM_VMS)/, $(patsubst %.c,%.o, $(notdir $(SRC_VMM_VMS))))
OBJ_DRIVERS	+= $(addprefix $(OUT_DRIVERS)/, $(patsubst %.c,%.o, $(notdir $(SRC_DRIVERS))))

OBJECT		= $(OUT_ARCH)/boot.o $(OBJ_ARCH) $(OBJ_CORE) $(OBJ_VMM_VMS) $(OBJ_DRIVERS)

all: $(OUT) $(OUT_CORE) $(OUT_ARCH) $(OUT_DRIVERS)  $(OUT_VMM_VMS) $(vmm_bin)

$(vmm_bin) : $(vmm_elf)
	$(QUIET) $(OBJ_COPY) -O binary $(vmm_elf) $(vmm_bin)
	$(QUIET) $(OBJ_DUMP) $(vmm_elf) -D > $(vmm_dump)

$(vmm_elf) : $(OBJECT) $(TARGET_LDS)
	@echo Linking $@
	$(QUIET) $(LD) $(LDFLAG) -o $(vmm_elf) $(OBJECT) $(LDPATH)
	@echo Done.

$(TARGET_LDS) : $(LDS) $(INCLUDE_DIR)
	@echo Generate LDS file
	$(QUIET) $(CC) $(CCFLAG) -E -P $(LDS) -o $(TARGET_LDS)

$(OUT) $(OUT_CORE) $(OUT_ARCH) $(OUT_VMM_VMS) $(OUT_DRIVERS):
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

$(OUT_VMM_VMS)/%.o: %.c $(INCLUDE_DIR)
	$(PROGRESS)
	$(QUIET) $(CC) $(CCFLAG) -c $< -o $@

$(OUT_DRIVERS)/%.o: %.c $(INCLUDE_DIR)
	$(PROGRESS)
	$(QUIET) $(CC) $(CCFLAG) -c $< -o $@

.PHONY: clean run app

clean:
	@ rm -rf out
	@ echo "All build has been cleaned"
