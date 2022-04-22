#ifndef __LINUX_MINOS_H__
#define __LINUX_MINOS_H_

#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/arm-smccc.h>
#include <linux/mmu_notifier.h>

#include "minos_hypercall.h"
#include "minos_ioctl.h"

#define MINOS_VM_MAX	(64)
#define VM_NAME_SIZE	32
#define VM_TYPE_SIZE	16

struct vmtag {
	uint32_t vmid;
	char name[VM_NAME_SIZE];
	char os_type[VM_TYPE_SIZE];
	int32_t nr_vcpu;
	uint64_t mem_base;
	uint64_t mem_size;
	uint64_t entry;
	uint64_t setup_data;
	uint64_t flags;
	uint32_t vcpu_affinity[8];
};

struct vm_device {
	int vmid;
	atomic_t opened;
	phys_addr_t pmem_map;
	unsigned long vm0_mmap_base;
	unsigned long map_size;
	unsigned long guest_page_size;
	struct list_head list;
	struct vmtag vmtag;
	struct device *parent;
	struct device device;
	struct file_operations *fops;
	struct task_struct *owner;
	struct list_head event_list;
	struct mmu_notifier mmu_notifier;
	struct vm_area_struct *guest_vma;
	struct mm_struct *mm;
};

struct vm_ring {
	volatile uint32_t ridx;
	volatile uint32_t widx;
	uint32_t size;
	char buf[0];
};

#define VM_RING_IDX(idx, size)	(idx & (size - 1))

#define MVM_EVENT_ID_BASE	(32)
#define MVM_MAX_EVENT		(512)
#define MVM_EVENT_ID_END	(MVM_EVENT_ID_BASE + MVM_MAX_EVENT)

int get_dynamic_virq(int irq);

#endif
