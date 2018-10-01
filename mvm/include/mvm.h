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
#include <sys/uio.h>
#include <linux/netlink.h>

#include <mvm_ioctl.h>
#include <compiler.h>
#include <vmcs.h>
#include <barrier.h>
#include <mvm_queue.h>

#define MAXCOMLEN	(19)

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

#define pr_debug(...)	\
	do {			\
		if (verbose)	\
			printf("[DEBUG] " __VA_ARGS__); \
	} while (0)

#define pr_err(...)	printf("[ERROR] " __VA_ARGS__)
#define pr_info(...)	printf("[INFO ] " __VA_ARGS__)
#define pr_warn(...)	printf("[WARN ] " __VA_ARGS__)

struct vm;

struct vm_os {
	char *name;
	int (*early_init)(struct vm *vm);
	int (*load_image)(struct vm *vm);
	int (*setup_vm_env)(struct vm *vm, char *cmdline);
};

#define OS_TYPE_LINUX		(1 << 0)

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
	struct vm_os *os;
	void *os_data;
	void *mmap;

	/* information of the vm */
	char name[32];
	char os_type[32];
	int32_t nr_vcpus;
	int bit64;
	uint64_t mem_size;
	uint64_t mem_start;
	uint64_t entry;
	uint64_t setup_data;
	uint64_t hvm_paddr;

	struct mvm_queue queue;

	void *vmcs;
	int *eventfds;
	int *epfds;
	int *irqs;

	struct list_head vdev_list;
};

extern struct vm *mvm_vm;

#define gpa_to_hvm_va(gpa) \
	(unsigned long)(mvm_vm->mmap + ((gpa) - mvm_vm->mem_start))

#define PAGE_SIZE			(4096)

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

#define VM_MAX_VCPUS			(8)

#define VMCS_SIZE(nr) 	BALIGN(nr * sizeof(struct vmcs), PAGE_SIZE)

void *map_vm_memory(struct vm *vm);

static inline void send_virq_to_vm(int virq)
{
	ioctl(mvm_vm->vm_fd, IOCTL_SEND_VIRQ, (long)virq);
}

void *hvm_map_iomem(void *base, size_t size);

#endif
