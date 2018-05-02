/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MVISOR_VM_H_
#define _MVISOR_VM_H_

#include <mvisor/types.h>
#include <mvisor/list.h>
#include <config/config.h>
#include <mvisor/mmu.h>
#include <mvisor/mm.h>

#define MVISOR_VM_NAME_SIZE	32

struct mvisor_vcpu;

struct os {

};

typedef void (*boot_vcpu_t)(void *arg);

extern struct list_head vm_list;

struct vm {
	uint32_t vmid;
	uint32_t vcpu_nr;
	uint32_t mmu_on;
	uint32_t index;
	uint32_t vcpu_affinity[CONFIG_VM_MAX_VCPU];
	unsigned long entry_point;
	char name[MVISOR_VM_NAME_SIZE];
	struct vcpu *vcpus[CONFIG_VM_MAX_VCPU];
	struct mm_struct mm;
	struct os os;
	struct list_head vm_list;
	/*
	 * each vm may have its own stage2 memory map
	 * to control the memory access
	 */
	boot_vcpu_t boot_vcpu;
} __align(sizeof(unsigned long));

#define for_each_vm(vm)	\
	list_for_each_entry(vm, &vm_list, vm_list)

struct vm *mvisor_get_vm(uint32_t vmid);

int mvisor_vms_init(void);
void vm_mm_struct_init(struct vm *vm);
int vm_memory_init(struct vm *vm);

#endif
