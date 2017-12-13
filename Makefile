ARCH 		:= armv8
CROSS_COMPILE 	:= aarch64-elf-
CC 		:= $(CROSS_COMPILE)gcc
LD 		:= $(CROSS_COMPILE)ld
OBJ_COPY	:= $(CROSS_COMPILE)objcopy
OBJ_DUMP 	:= $(CROSS_COMPILE)objdump

#PLATFORM	:= fvp
#BOARD		:= armv8-fvp

INCLUDE_DIR 	:= include/core/*.h include/asm/*.h include/config/*.h
#INCLUDE_DIR 	:=

CCFLAG 		:= --static -nostdlib -fno-builtin -g -fno-pic -I$(PWD)/include
LDS 		:= arch/$(ARCH)/lds/vmm.lds
LDFLAG 		:= -T$(LDS) -Map=linkmap.txt
#LDPATH 		:= -L/opt/i686-linux-android-4.6/lib/gcc/i686-linux-android/4.6.x-google

OUT 		:= out
OUT_CORE 	= $(OUT)/core
OUT_ARCH 	= $(OUT)/$(ARCH)
OUT_VMM_VMS	= $(OUT)/vmm_vms

vmm_elf 	:= $(OUT)/vmm.elf
vmm_bin 	:= $(OUT)/vmm.bin
vmm_dump 	:= $(OUT)/vmm.s

SRC_ARCH_C	:= $(wildcard arch/$(ARCH)/*.c)
SRC_ARCH_S	:= $(wildcard arch/$(ARCH)/*.S)
SRC_CORE	:= $(wildcard core/*.c)
SRC_VMM_VMS	:= $(wildcard vmm_vms/*.c)

#VPATH		:= kernel:mm:init:fs:drivers:syscall:arch/$(ARCH)/platform/$(PLATFORM):arch/$(ARCH)/board/$(BOARD):arch/$(ARCH)/kernel
VPATH		:= core:arch/$(ARCH):vmm_vms

.SUFFIXES:
.SUFFIXES: .S .c

_OBJ_ARCH	+= $(addprefix $(OUT_ARCH)/, $(patsubst %.c,%.o, $(notdir $(SRC_ARCH_C))))
_OBJ_ARCH	+= $(addprefix $(OUT_ARCH)/, $(patsubst %.S,%.o, $(notdir $(SRC_ARCH_S))))
OBJ_ARCH	= $(subst out/$(ARCH)/boot.o,,$(_OBJ_ARCH))
OBJ_CORE	+= $(addprefix $(OUT_CORE)/, $(patsubst %.c,%.o, $(notdir $(SRC_CORE))))
OBJ_VMM_VMS	+= $(addprefix $(OUT_VMM_VMS)/, $(patsubst %.c,%.o, $(notdir $(SRC_VMM_VMS))))

OBJECT		= $(OUT_ARCH)/boot.o $(OBJ_ARCH) $(OBJ_CORE) $(OBJ_VMM_VMS)

all: $(OUT) $(OUT_CORE) $(OUT_ARCH) $(OUT_VMM_VMS) $(vmm_bin)

$(vmm_bin) : $(vmm_elf)
	$(OBJ_COPY) -O binary $(vmm_elf) $(vmm_bin)
	$(OBJ_DUMP) $(vmm_elf) -D > $(vmm_dump)

$(vmm_elf) : $(OBJECT) $(LDS)
	$(LD) $(LDFLAG) -o $(vmm_elf) $(OBJECT) $(LDPATH)

$(OUT) $(OUT_CORE) $(OUT_ARCH) $(OUT_VMM_VMS):
	@ mkdir -p $@

$(OUT_ARCH)/%.o: %.c $(INCLUDE_DIR)
	$(CC) $(CCFLAG) -c $< -o $@

$(OUT_ARCH)/boot.o: arch/$(ARCH)/boot.S $(INCLUDE_DIR)
	$(CC) $(CCFLAG) -c arch/$(ARCH)/boot.S -o $@

$(OUT_ARCH)/%.o: %.S $(INCLUDE_DIR) 
	$(CC) $(CCFLAG) -c $< -o $@

$(OUT_CORE)/%.o: %.c $(INCLUDE_DIR)
	$(CC) $(CCFLAG) -c $< -o $@

$(OUT_VMM_VMS)/%.o: %.c $(INCLUDE_DIR)
	$(CC) $(CCFLAG) -c $< -o $@

.PHONY: clean run app

clean:
	@ rm -rf out
	@ echo "All build has been cleaned"
