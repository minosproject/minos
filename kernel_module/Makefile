ARCH 		:= arm64
CROSS_COMPILE 	:= aarch64-linux-gnu-
CC 		:= $(CROSS_COMPILE)gcc
LD 		:= $(CROSS_COMPILE)ld
OBJ_COPY	:= $(CROSS_COMPILE)objcopy
OBJ_DUMP 	:= $(CROSS_COMPILE)objdump
NM		:= $(CROSS_COMPILE)nm
STRIP		:= $(CROSS_COMPILE)strip

obj-m		:= mvm.o
mvm-objs	:= hvc.o minos.o
PWD		:= $(shell pwd)

KERN_DIR	:= ~/work/github/mvisor-test/linux-linaro-stable

all:
	$(MAKE) -C $(KERN_DIR) ARCH=$(ARCH) M=$(PWD) modules

.PHONY: clean
clean:
	rm -f *.o
	rm -f *.symvers
	rm -f *.order
	rm -f *.ko
	rm -f *.mod.c
