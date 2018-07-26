ARCH 		:= aarch64
CROSS_COMPILE 	:= aarch64-linux-gnu-
CC 		:= $(CROSS_COMPILE)gcc
LD 		:= $(CROSS_COMPILE)ld
OBJ_COPY	:= $(CROSS_COMPILE)objcopy
OBJ_DUMP 	:= $(CROSS_COMPILE)objdump
NM		:= $(CROSS_COMPILE)nm
STRIP		:= $(CROSS_COMPILE)strip

QUIET ?= @

ifeq ($(QUIET),@)
PROGRESS = @echo Compiling $< ...
endif

TARGET := mvm

CFLAG := --static

src	:= mvm.c
inc	:= mvm.h

$(TARGET) : $(src) $(inc)
	$(PROGRESS)
	$(QUIET) $(CC) $(CFLAG) $(src) -o $(TARGET)
	$(QUIET) $(STRIP) -s $(TARGET)


.PHONY: clean
	$(QUIET) rm $(TARGET)
