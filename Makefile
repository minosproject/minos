ARCH 		:= aarch64
CROSS_COMPILE 	:= aarch64-elf-
CC 		:= $(CROSS_COMPILE)gcc
LD 		:= $(CROSS_COMPILE)ld
OBJ_COPY	:= $(CROSS_COMPILE)objcopy
OBJ_DUMP 	:= $(CROSS_COMPILE)objdump

PLATFORM	:= fvp
TARGET		:= mvisor

QUIET ?= @

include build/$(PLATFORM).mk

src_dir-y			+= mvisor arch/$(ARCH) mvisor_vms
src_dir-$(CONFIG_UART_PL011)	+= drivers/pl011
src_dir-$(CONFIG_LIBFDT)	+= external/libfdt

inc_dir-y			+= include/mvisor include/asm include/config
inc_dir-$(CONFIG_LIBFDT)	+= include/libfdt

inc_h-y		= $(shell find $(inc_dir-y) -type f -name "*.h")
inc_s-y		+= $(shell find $(inc_dir-y) -type f -name "*.S")
src_c-y		= $(shell find $(src_dir-y) -type f -name "*.c")
src_s-y		+= $(shell find $(src_dir-y) -type f -name "*.S")

LDS_SRC		= lds/$(ARCH).ld.c

.SUFFIXES:
.SUFFIXES: .S .c

OUT		:= out
TARGET_ELF	:= $(OUT)/$(TARGET).elf
TARGET_BIN	:= $(OUT)/$(TARGET).bin
TARGET_LDS	:= $(OUT)/$(ARCH).lds
TARGET_DUMP	:= $(OUT)/mvisor.s

INCLUDE_DIR 	:= $(inc_dir-y)
CCFLAG 		:= -Wall --static -nostdlib -fno-builtin -g \
		   -march=armv8-a -I$(PWD)/include
LDFLAG 		:= -T$(TARGET_LDS) -Map=$(OUT)/linkmap.txt

obj-out-dir	:= $(addprefix $(OUT)/, $(src_dir-y))

VPATH		:= $(src_dir-y)

objs		= $(src_c-y:%.c=$(OUT)/%.o)
objs-s		= $(src_s-y:%.S=$(OUT)/%.O)

ifeq ($(QUIET),@)
PROGRESS = @echo Compiling $< ...
endif

all: $(obj-out-dir) $(TARGET_BIN)

$(TARGET_BIN) : $(TARGET_ELF)
	$(QUIET) $(OBJ_COPY) -O binary $(TARGET_ELF) $(TARGET_BIN)
	$(QUIET) $(OBJ_DUMP) $(TARGET_ELF) -D > $(TARGET_DUMP)
	$(QUIET) echo "Build done sucessfully"

$(TARGET_ELF) : include/asm  $(objs) $(objs-s) $(TARGET_LDS)
	@ echo Linking $@ ...
	$(QUIET) $(LD) $(LDFLAG) -o $(TARGET_ELF) $(objs-s) $(objs) $(LDPATH)
	@ echo Linking done

$(TARGET_LDS) : $(LDS_SRC)
	@ echo Generate LDS file ...
	$(QUIET) $(CC) $(CCFLAG) -E -P $(LDS_SRC) -o $(TARGET_LDS)
	@ echo Generate LDS file done

$(obj-out-dir) :
	@ mkdir -p $@

include/asm :
	@ echo Link asm-$(ARCH) to include/asm ...
	@ ln -s asm-$(ARCH) include/asm

$(OUT)/%.o : %.c $(INCLUDE_DIR)
	$(PROGRESS)
	$(QUIET) $(CC) $(CCFLAG) -c $< -o $@

$(OUT)/%.O : %.S $(INCLUDE_DIR)
	$(PROGRESS)
	$(QUIET) $(CC) $(CCFLAG) -c $< -o $@

.PHONY: clean

clean:
	@ rm -rf out
	@ rm include/asm
	@ echo "All build objects have been cleaned"
