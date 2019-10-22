#ifndef __VM_H__
#define __VM_H__

#include <mvm_queue.h>
#include <list.h>
#include <sys/ioctl.h>
#include <common/hypervisor.h>

#define VM_STAT_RUNNING			0x0
#define VM_STAT_SUSPEND			0x1

#define VM_MAX_DEVICES	(10)

struct device_info {
	int nr_device;
	int nr_virtio_dev;
	char *device_args[VM_MAX_DEVICES];
};

struct vm_config {
	int gic_type;
	struct vmtag vmtag;
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
	unsigned long mem_size;
	unsigned long mem_start;
	unsigned long entry;
	unsigned long setup_data;
	unsigned long hvm_paddr;

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

static inline int request_virq(unsigned long flags)
{
	return ioctl(mvm_vm->vm_fd, IOCTL_REQUEST_VIRQ, flags);
}

#endif
