#ifndef __MINOS_USER_H__
#define __MINOS_USER_H__

#define MINOS_IOCTL_CREATE_VM		(0xf001)
#define MINOS_IOCTL_DESTORY_VM		(0xf002)
#define MINOS_IOCTL_RESTART_VM		(0xf003)
#define MINOS_IOCTL_POWER_DOWN_VM	(0xf004)
#define MINOS_IOCTL_POWER_UP_VM		(0xf005)
#define MINOS_IOCTL_MMAP		(0xf006)
#define MINOS_IOCTL_UNMMAP		(0xf007)

struct vm_info {
	int8_t name[32];
	int8_t os_type[32];
	int32_t nr_vcpus;
	int32_t bit64;
	uint64_t mem_size;
	uint64_t mem_start;
	uint64_t entry;
	uint64_t setup_data;
};

#define VM_MEM_START			(0x80000000UL)
#define MEM_BLOCK_SIZE			(0x200000)
#define MEM_BLOCK_SHIFT			(21)
#define VM_MAX_MMAP_SIZE		(MEM_BLOCK_SIZE * 8)

#define MEM_BLOCK_ALIGN(v)		(v & ~(MEM_BLOCK_SIZE - 1))
#define MEM_BLOCK_BALIGN(v) \
	((v + MEM_BLOCK_SIZE - 1) & ~(MEM_BLOCK_SIZE - 1))

#define VM_MAX_VCPUS			(2)

#endif
