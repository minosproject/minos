#ifndef __VM_H__
#define __VM_H__

#include <sys/ioctl.h>

#include <minos/mvm.h>
#include <minos/compiler.h>
#include <minos/vmcs.h>
#include <minos/barrier.h>
#include <minos/mvm_queue.h>
#include <minos/debug.h>
#include <minos/list.h>
#include <minos/os.h>
#include <common/hypervisor.h>

struct vm;

/*
 * cpuid : vcpu_id of this vcpu in the vm
 * vm : the VM which this vcpu belongs to
 * vmcs : shared memory used to commuicate with
 *        hypervisor
 *
 * event_fd : kernel will use event_fd to send event to
 *            the vcpu thread
 * epoll_fd : process send event to thread
 * vcpu_irq : each vcpu will have a virq, hypervisor can
 *            send the event via this virq
 */
struct vcpu {
	int cpuid;
	struct vm *vm;
	void *vmcs;

	int event_fd;
	int epoll_fd;
	int vcpu_irq;

	pthread_t vcpu_thread;
};

/*
 * vmid	 : vmid allocated by hypervisor
 * vm_fd : fd of this VM when open this VM
 * image_fd : bootimage fd if using bootimg
 * kfd : kernel image fd
 * dfd : device tree image fd
 * rfd : ramdisk image fd
 * state : the state of this VM
 * flags : flags of this VM
 * os : the operating system of this VM
 * os_data : os private data of this VM
 * mmap : memory space mapped to the processer
 *
 * vcpus : all vcpus of this VM
 */
struct vm {
	int vmid;
	int vm_fd;
	int vm0_fd;
	int image_fd;
	int kfd;
	int dfd;
	int rfd;
	int state;

	int32_t nr_vcpus;
	struct vcpu **vcpus;

	uint64_t flags;

	struct vm_os *os;
	void *os_data;

	void *mmap;

	/* information of the vm */
	char name[VM_NAME_SIZE];
	char *cmdline;

	uint64_t mem_size;
	uint64_t mem_start;
	uint64_t map_start;
	uint64_t map_size;
	uint64_t entry;
	uint64_t setup_data;

	int gic_type;

	struct mvm_queue queue;

	struct list_head vdev_list;

	struct list_head vmm_area_free;
	struct list_head vmm_area_used;
};

extern struct vm *mvm_vm;

#define gpa_to_hvm_va(gpa) \
	(unsigned long)(mvm_vm->mmap + ((gpa) - mvm_vm->mem_start))

void *map_vm_memory(struct vm *vm);
void *hvm_map_iomem(unsigned long base, size_t size);

static inline void send_virq_to_vm(int virq)
{
	ioctl(mvm_vm->vm_fd, IOCTL_SEND_VIRQ, (long)virq);
}

static inline int request_virq(unsigned long flags)
{
	return ioctl(mvm_vm->vm_fd, IOCTL_REQUEST_VIRQ, flags);
}

#endif
