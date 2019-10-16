#ifndef __MINOS_HYPERVISOR_H__
#define __MINOS_HYPERVISOR_H__

#ifdef BUILD_HYPERVISOR
#include <minos/types.h>
#else
#include <inttypes.h>
#include <sys/types.h>
#endif

#define VM_NAME_SIZE	32
#define VM_TYPE_SIZE	16

#define VM_FLAGS_64BIT			(1 << 0)
#define VM_FLAGS_NATIVE			(1 << 1)
#define VM_FLAGS_DYNAMIC_AFF		(1 << 2)
#define VM_FLAGS_NO_RAMDISK		(1 << 3)
#define VM_FLAGS_NO_BOOTIMAGE		(1 << 4)
#define VM_FLAGS_HAS_EARLYPRINTK	(1 << 5)

#define VM_FLAGS_SETUP_OF		(1 << 8)
#define VM_FLAGS_SETUP_ACPI		(1 << 9)
#define VM_FLAGS_SETUP_ATAG		(1 << 10)
#define VM_FLAGS_SETUP_OTHER		(1 << 11)
#define VM_FLAGS_SETUP_MASK		(0xf00)

struct vmtag {
	uint32_t vmid;
	char name[VM_NAME_SIZE];
	char os_type[VM_TYPE_SIZE];
	int32_t nr_vcpu;
	unsigned long mem_base;
	unsigned long mem_size;
	void *entry;
	void *setup_data;
	unsigned long flags;
	uint32_t vcpu_affinity[8];
};

#define IOCTL_CREATE_VM			0xf000
#define IOCTL_DESTROY_VM		0xf001
#define IOCTL_RESTART_VM		0xf002
#define IOCTL_POWER_DOWN_VM		0xf003
#define IOCTL_POWER_UP_VM		0xf004
#define IOCTL_VM_MMAP			0xf005
#define IOCTL_VM_UNMAP			0xf006
#define IOCTL_REGISTER_VCPU		0xf007
#define IOCTL_SEND_VIRQ			0xf008
#define IOCTL_CREATE_VMCS		0xf00a
#define IOCTL_CREATE_VMCS_IRQ		0xf00b
#define IOCTL_UNREGISTER_VCPU		0xf00c
#define IOCTL_VIRTIO_MMIO_INIT		0xf00d
#define IOCTL_VIRTIO_MMIO_DEINIT	0xf00e
#define IOCTL_REQUEST_VIRQ		0xf00f
#define IOCTL_CREATE_HOST_VDEV		0xf010

#endif
