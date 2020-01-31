CROSS_COMPILE	:=

CC 		:= $(CROSS_COMPILE)gcc
LD 		:= $(CROSS_COMPILE)ld
OBJ_COPY	:= $(CROSS_COMPILE)objcopy
OBJ_DUMP 	:= $(CROSS_COMPILE)objdump
NM		:= $(CROSS_COMPILE)nm
STRIP		:= $(CROSS_COMPILE)strip

PWD		:= $(shell pwd)

QUIET ?= @

ifeq ($(QUIET),@)
PROGRESS = @echo Compiling $@ ...
endif

TARGET := parse_dtb

CCFLAG := -Wall -D_XOPEN_SOURCE -D_GNU_SOURCE \
	-Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing \
	-fno-common -Werror-implicit-function-declaration \
	-Wno-format-security -I$(PWD)

src	:= fdt_parse.c
src	+= libfdt/fdt_addresses.c libfdt/fdt.c libfdt/fdt_empty_tree.c
src	+= libfdt/fdt_ro.c libfdt/fdt_rw.c libfdt/fdt_strerror.c
src	+= libfdt/fdt_sw.c libfdt/fdt_wip.c libfdt/fdt_overlay.c

INCLUDE_DIR = of.h

objs	:= $(src:%.c=%.o)

$(TARGET) : $(objs)
	$(PROGRESS)
	$(QUIET) $(CC) $^ -o $@ $(CCFLAG) -lpthread
	$(QUIET) $(STRIP) -s $(TARGET)

%.o : %.c $(INCLUDE_DIR) Makefile
	$(PROGRESS)
	$(QUIET) $(CC) $(CCFLAG) -c $< -o $@

.PHONY: clean

clean:
	$(QUIET) rm -rf $(TARGET) $(objs)
