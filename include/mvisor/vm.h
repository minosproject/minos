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

#define VMM_VM_NAME_SIZE	32

struct vmm_vcpu;

struct os {

};

typedef int (*boot_vm_t)(void *arg);

extern struct list_head vm_list;

typedef struct vmm_vm {
	uint32_t vmid;
	uint32_t vcpu_nr;
	uint32_t mmu_on;
	uint32_t index;
	uint32_t vcpu_affinity[CONFIG_VM_MAX_VCPU];
	unsigned long entry_point;
	char name[VMM_VM_NAME_SIZE];
	struct vmm_vcpu *vcpus[CONFIG_VM_MAX_VCPU];
	struct mm_struct mm;
	struct os os;
	struct list_head vm_list;
	/*
	 * each vm may have its own stage2 memory map
	 * to control the memory access
	 */
	boot_vm_t boot_vm;
} vm_t __attribute__((__aligned__ (8)));

vm_t *vmm_get_vm(uint32_t vmid);

int vmm_vms_init(void);
void vm_mm_struct_init(vm_t *vm);
int vm_memory_init(vm_t *vm);

#endif
