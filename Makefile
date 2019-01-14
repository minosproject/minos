all: hypervisor mvm

ARCH ?= aarch64
CROSS_COMPILE ?= aarch64-linux-gnu-

.PHONY: hypervisor mvm clean distclean

hypervisor:
	@ echo "build hypervisor"
	@ cd hypervisor && make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) all

mvm:
	@ echo "build minos userspace tools"
	@ cd mvm && make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)

clean:
	@ echo "clean all things"
	@ cd hypervisor && make clean
	@ cd mvm && make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean

distclean:
	@ echo "clean all things"
	@ cd hypervisor && make distclean
	@ cd mvm && make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean

%defconfig:
	@ cd hypervisor && make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) $@

dtbs:
	@ cd hypervisor && make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) $@
