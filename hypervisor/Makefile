ARCH 		:= aarch64
CROSS_COMPILE 	:= aarch64-linux-gnu-
CC 		:= $(CROSS_COMPILE)gcc
LD 		:= $(CROSS_COMPILE)ld
OBJ_COPY	:= $(CROSS_COMPILE)objcopy
OBJ_DUMP 	:= $(CROSS_COMPILE)objdump
NM		:= $(CROSS_COMPILE)nm
STRIP		:= $(CROSS_COMPILE)strip

PLATFORM	:= espressobin
TARGET		:= minos
DEBUG_LEVEL	:= 0

QUIET ?= @

include build/$(PLATFORM).mk

src_dir-y				+= minos arch/$(ARCH) platform/$(PLATFORM)
src_dir-$(CONFIG_SERIAL_PL011)		+= drivers/serial_pl011
src_dir-$(CONFIG_SERIAL_MVEBU_A3700)	+= drivers/serial_mvebu_a3700
src_dir-$(CONFIG_LIBFDT)		+= external/libfdt
src_dir-$(CONFIG_JSON_SJSON)		+= external/sjson
src_dir-y				+= drivers/virtio

inc_dir-y				+= include/minos include/asm include/config config/$(PLATFORM)
inc_dir-$(CONFIG_LIBFDT)		+= include/libfdt
inc_dir-$(CONFIG_JSON_SJSON)		+= include/sjson

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
TARGET_DUMP	:= $(OUT)/$(TARGET).s
TARGET_OBJECT	:= $(OUT)/__vminos__.o

INCLUDE_DIR 	:= $(inc_dir-y)
CCFLAG 		:= -Wall -O$(DEBUG_LEVEL) --static -nostdlib -fno-builtin -g \
		   -march=armv8-a -I$(PWD)/include
LDFLAG 		:= -T$(TARGET_LDS) -Map=$(OUT)/linkmap.txt

obj-out-dir	:= $(addprefix $(OUT)/, $(src_dir-y))

VPATH		:= $(src_dir-y)

objs		= $(src_c-y:%.c=$(OUT)/%.o)
objs-s		= $(src_s-y:%.S=$(OUT)/%.O)
obj-symbols	= $(OUT)/allsymbols.o
obj-config	= $(OUT)/$(PLATFORM)_config.o

ifeq ($(QUIET),@)
PROGRESS = @echo Compiling $< ...
endif

all: $(obj-out-dir) $(TARGET_BIN)

$(TARGET_BIN) : $(TARGET_ELF)
	$(QUIET) $(OBJ_COPY) -O binary $(TARGET_ELF) $(TARGET_BIN)
	$(QUIET) $(OBJ_DUMP) $(TARGET_ELF) -D > $(TARGET_DUMP)
	$(QUIET) echo "Build done sucessfully"

$(TARGET_ELF) : include/asm include/config/config.h $(TARGET_OBJECT) $(obj-symbols) $(TARGET_LDS)
	@ echo Linking $@ ...
	$(QUIET) $(LD) $(LDFLAG) -o $(TARGET_ELF) $(TARGET_OBJECT) $(obj-symbols) $(LDPATH)

$(TARGET_LDS) : $(LDS_SRC)
	@ echo Generate LDS file ...
	$(QUIET) $(CC) $(CCFLAG) -E -P $(LDS_SRC) -o $(TARGET_LDS)

$(obj-symbols) : $(TARGET_OBJECT) $(TARGET_LDS) tools/generate_allsymbols.py
	@ echo Generate Symbols file
	$(QUIET) $(LD) $(LDFLAG) -o $(OUT)/tmp.minos.elf $(TARGET_OBJECT) $(LDPATH)
	$(QUIET) $(NM) -n $(OUT)/tmp.minos.elf > $(OUT)/tmp.minos.symbols
	$(QUIET) python3 tools/generate_allsymbols.py $(OUT)/tmp.minos.symbols $(OUT)/allsymbols.S
	@ echo Compiling allsymbols.S ...
	$(QUIET) $(CC) $(CCFLAG) -c $(OUT)/allsymbols.S -o $(obj-symbols)

$(TARGET_OBJECT) : $(objs) $(objs-s) $(obj-config)
	@ echo Linking $@ ...
	$(QUIET) $(LD) -r -o $(TARGET_OBJECT) $(objs) $(objs-s) $(obj-config)

$(obj-out-dir) :
	@ mkdir -p $@

include/asm :
	@ echo Link asm-$(ARCH) to include/asm ...
	@ ln -s asm-$(ARCH) include/asm

include/config/config.h:
	@ echo Link config/$(PLATFORM)/$(PLATFORM)_config.h to include/config/config.h ...
	@ mkdir -p include/config
	@ ln -s ../../config/$(PLATFORM)/$(PLATFORM)_config.h include/config/config.h

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
	@ echo "    "
	@ echo "****** Generate $(PLATFORM)_config.c ******"
	$(QUIET) python3 tools/generate_mvconfig.py $< $@
	@ echo "****** Generate $(PLATFORM)_config.c done ******"
	@ echo "    "

$(OUT)/$(PLATFORM).json : config/$(PLATFORM)/$(PLATFORM).json.cc config/$(PLATFORM)/$(PLATFORM)_config.h config/$(PLATFORM)/*.cc include/minos/virt.h
	$(PROGRESS)
	$(QUIET) $(CC) $(CCFLAG) -E -P config/$(PLATFORM)/$(PLATFORM).json.cc -o $(OUT)/$(PLATFORM).json

.PHONY: clean distclean

clean:
	@ echo "removing objs and out directory ..."
	@ rm -rf out
	@ echo "All build objects have been cleaned"

distclean:
	@ echo "removing objs and out directory ..."
	@ rm -rf out include/config
	@ echo "removing include/asm directory ..."
	@ rm -rf include/asm
	@ echo "removing include/config/config.h ..."
	@ rm -rf include/config/config.h
	@ echo "All build objects have been cleaned"
