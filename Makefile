ARCH 		:= aarch64
CROSS_COMPILE 	:= aarch64-linux-gnu-
CC 		:= $(CROSS_COMPILE)gcc
LD 		:= $(CROSS_COMPILE)ld
OBJ_COPY	:= $(CROSS_COMPILE)objcopy
OBJ_DUMP 	:= $(CROSS_COMPILE)objdump

PLATFORM	:= fvp
TARGET		:= minos

QUIET ?= @

include build/$(PLATFORM).mk

src_dir-y			+= minos virt arch/$(ARCH) platform/$(PLATFORM)
src_dir-$(CONFIG_UART_PL011)	+= drivers/pl011
src_dir-$(CONFIG_LIBFDT)	+= external/libfdt
src_dir-$(CONFIG_JSON_SJSON)	+= external/sjson

inc_dir-y			+= include/minos include/virt include/asm include/config config/$(PLATFORM)
inc_dir-$(CONFIG_LIBFDT)	+= include/libfdt
inc_dir-$(CONFIG_JSON_SJSON)	+= include/sjson

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
TARGET_DUMP	:= $(OUT)/minos.s

INCLUDE_DIR 	:= $(inc_dir-y)
CCFLAG 		:= -Wall --static -nostdlib -fno-builtin -g \
		   -march=armv8-a -I$(PWD)/include
LDFLAG 		:= -T$(TARGET_LDS) -Map=$(OUT)/linkmap.txt

obj-out-dir	:= $(addprefix $(OUT)/, $(src_dir-y))

VPATH		:= $(src_dir-y)

objs		= $(src_c-y:%.c=$(OUT)/%.o)
objs-s		= $(src_s-y:%.S=$(OUT)/%.O)

obj-config	= $(OUT)/$(PLATFORM)_config.o

ifeq ($(QUIET),@)
PROGRESS = @echo Compiling $< ...
endif

all: $(obj-out-dir) $(TARGET_BIN)

$(TARGET_BIN) : $(TARGET_ELF)
	$(QUIET) $(OBJ_COPY) -O binary $(TARGET_ELF) $(TARGET_BIN)
	$(QUIET) $(OBJ_DUMP) $(TARGET_ELF) -D > $(TARGET_DUMP)
	$(QUIET) echo "Build done sucessfully"

$(TARGET_ELF) : include/asm include/config/config.h $(objs) $(objs-s) $(obj-config) $(TARGET_LDS)
	@ echo Linking $@ ...
	$(QUIET) $(LD) $(LDFLAG) -o $(TARGET_ELF) $(objs-s) $(objs) $(obj-config) $(LDPATH)
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

include/config/config.h:
	@ echo Link config/$(PLATFORM)/$(PLATFORM)_config.h to include/config/config.h ...
	@  ln -s ../../config/$(PLATFORM)/$(PLATFORM)_config.h include/config/config.h

$(obj-config): $(OUT)/$(PLATFORM)_config.c
	$(PROGRESS)
	$(QUIET) $(CC) $(CCFLAG) -c $< -o $@

$(OUT)/%.o : %.c $(INCLUDE_DIR)
	$(PROGRESS)
	$(QUIET) $(CC) $(CCFLAG) -c $< -o $@

$(OUT)/%.O : %.S $(INCLUDE_DIR)
	$(PROGRESS)
	$(QUIET) $(CC) $(CCFLAG) -c $< -o $@

$(OUT)/$(PLATFORM)_config.c : $(OUT)/$(PLATFORM).json
	@ echo "****** Generate $(PLATFORM)_config.c ******"
	$(QUIET) python3 tools/generate_mvconfig.py $< $@
	@ echo "****** Generate $(PLATFORM)_config.c done ******"

$(OUT)/$(PLATFORM).json : config/$(PLATFORM)/$(PLATFORM).json.cc config/$(PLATFORM)/$(PLATFORM)_config.h config/$(PLATFORM)/*.cc include/virt/virt.h
	$(PROGRESS)
	$(QUIET) $(CC) $(CCFLAG) -E -P config/$(PLATFORM)/$(PLATFORM).json.cc -o $(OUT)/$(PLATFORM).json

.PHONY: clean

clean:
	@ echo "removing objs and out directory ..."
	@ rm -rf out
	@ echo "removing include/asm directory ..."
	@ rm -rf include/asm
	@ echo "removing include/config/config.h ..."
	@ rm -rf include/config/config.h
	@ echo "All build objects have been cleaned"
