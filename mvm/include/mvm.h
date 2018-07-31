#ifndef __MINOS_USER_H__
#define __MINOS_USER_H__

#define IOCTL_CREATE_VM			(0xf000)
#define IOCTL_DESTORY_VM		(0xf001)
#define IOCTL_RESTART_VM		(0xf002)
#define IOCTL_POWER_DOWN_VM		(0xf003)
#define IOCTL_POWER_UP_VM		(0xf004)
#define IOCTL_VM_MMAP			(0xf005)
#define IOCTL_VM_UNMAP			(0xf006)
#define IOCTL_VM_MMAP_INFO		(0xf007)

#define IOCTL_GET_VM_MMAP_SIZE		(0xf100)

/*
 * MVM_FLAGS_NO_RAMDISK - used for linux to indicate
 * that the system has no ramdisk image
 */
#define MVM_FLAGS_NO_RAMDISK		(1 << 0)

extern int verbose;

#define printv(fmt, arg...)	\
	do {			\
		if (verbose)	\
			printf(fmt, ##arg); \
	} while (0)

struct vm_info {
	char name[32];
	char os_type[32];
	int32_t nr_vcpus;
	int32_t bit64;
	uint64_t mem_size;
	uint64_t mem_start;
	uint64_t entry;
	uint64_t setup_data;
};

struct vm;

struct vm_os {
	char *name;
	int (*early_init)(struct vm *vm);
	int (*load_image)(struct vm *vm);
	int (*setup_vm_env)(struct vm *vm);
};

/*
 * vmid	 : vmid allocated by hypervisor
 * flags : some flags of this vm
 * os	 : the os of this vm
 */
struct vm {
	int vmid;
	int vm_fd;
	int image_fd;
	unsigned long flags;
	struct vm_info vm_info;
	struct vm_os *os;
	void *os_data;
	void *mmap;
	unsigned long mmap_size;
};

#define VM_MEM_START			(0x80000000UL)
#define VM_MIN_MEM_SIZE			(32 * 1024 * 1024)
#define MEM_BLOCK_SIZE			(0x200000)
#define MEM_BLOCK_SHIFT			(21)
#define VM_MAX_MMAP_SIZE		(MEM_BLOCK_SIZE * 8)

#define MEM_BLOCK_ALIGN(v)		((v) & ~(MEM_BLOCK_SIZE - 1))
#define MEM_BLOCK_BALIGN(v) \
	(((v) + MEM_BLOCK_SIZE - 1) & ~(MEM_BLOCK_SIZE - 1))

#define VM_MAX_VCPUS			(2)

int map_vm_memory(int fd, uint64_t offset, uint64_t size);

#endif
