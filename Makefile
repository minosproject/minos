all: hypervisor kernel_module mvm

.PHONY: hypervisor kernel_module mvm clean distclean

hypervisor:
	@ echo "build hypervisor"
	@ cd hypervisor && make

kernel_module:
	@ echo "build kernel driver module"
	@ cd kernel_module && make

mvm:
	@ echo "build minos userspace tools"
	@ cd mvm && make

clean:
	@ echo "clean all things"
	@ cd hypervisor && make clean
	@ cd kernel_module && make clean
	@ cd mvm && make clean

distclean:
	@ echo "clean all things"
	@ cd hypervisor && make distclean
	@ cd kernel_module && make clean
	@ cd mvm && make clean
