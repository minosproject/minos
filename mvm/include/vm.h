#ifndef __VM_H__
#define __VM_H__

#include <mvm_queue.h>
#include <list.h>
#include <sys/ioctl.h>
#include <mvm_ioctl.h>

/*
 * VM_FLAGS_NO_RAMDISK - used for linux to indicate
 * that the system has no ramdisk image
 */
#define VM_FLAGS_NO_RAMDISK		(1 << 0)
#define VM_FLAGS_NO_BOOTIMAGE		(1 << 1)
#define VM_FLAGS_HAS_EARLYPRINTK	(1 << 2)

#define VM_STAT_RUNNING			0x0
#define VM_STAT_SUSPEND			0x1

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

#define VM_MAX_DEVICES	(10)

struct device_info {
	int nr_device;
	int nr_virtio_dev;
	char *device_args[VM_MAX_DEVICES];
};

struct vm_config {
	unsigned long flags;
	int gic_type;
	struct vm_info vm_info;
	struct device_info device_info;
	char bootimage_path[256];
	char cmdline[256];
	char kernel_image[256];
	char dtb_image[256];
	char ramdisk_image[256];
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
	int kfd;
	int dfd;
	int rfd;
	int state;
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

	struct vm_config *vm_config;
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

void *map_vm_memory(struct vm *vm);
void *hvm_map_iomem(void *base, size_t size);

static inline void send_virq_to_vm(int virq)
{
	ioctl(mvm_vm->vm_fd, IOCTL_SEND_VIRQ, (long)virq);
}

#endif
