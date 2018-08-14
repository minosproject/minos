#ifndef __MINOS_USER_H__
#define __MINOS_USER_H__

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <list.h>
#include <linux/netlink.h>

#include <mvm_ioctl.h>
#include <compiler.h>

/*
 * MVM_FLAGS_NO_RAMDISK - used for linux to indicate
 * that the system has no ramdisk image
 */
#define MVM_FLAGS_NO_RAMDISK		(1 << 0)

#define DEFINE_OS(os) \
	static void *os_##os __section("mvm_os") __used = &os;

extern void *__start_mvm_os;
extern void *__stop_mvm_os;

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
	uint64_t mmap_base;
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

	struct nlmsghdr *nlh;
	int sock_fd;
	int event_fd;
	int epfd;
};

#define VM_MEM_START			(0x80000000UL)
#define VM_MIN_MEM_SIZE			(64 * 1024 * 1024)
#define MEM_BLOCK_SIZE			(0x200000)
#define MEM_BLOCK_SHIFT			(21)
#define VM_MAX_MMAP_SIZE		(MEM_BLOCK_SIZE * 8)

#define ALIGN(v, s)			((v) & ~((s) - 1))
#define BALIGN(v, s)			(((v) + (s) - 1) & ~((s) - 1))

#define MEM_BLOCK_ALIGN(v)		((v) & ~(MEM_BLOCK_SIZE - 1))
#define MEM_BLOCK_BALIGN(v) \
	(((v) + MEM_BLOCK_SIZE - 1) & ~(MEM_BLOCK_SIZE - 1))

#define VM_MAX_VCPUS			(4)

void *map_vm_memory(struct vm *vm);

static inline void send_virq_to_vm(struct vm *vm, int virq)
{
	ioctl(vm->vm_fd, IOCTL_SEND_VIRQ, (long)virq);
}

#endif
