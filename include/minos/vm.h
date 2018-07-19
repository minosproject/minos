/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MINOS_VM_H_
#define _MINOS_VM_H_

#include <minos/types.h>
#include <minos/list.h>
#include <config/config.h>
#include <minos/mmu.h>
#include <minos/mm.h>

#define MINOS_VM_NAME_SIZE	32
#define OS_TYPE_SIZE		32

struct vcpu;
struct os;

extern struct list_head vm_list;

struct vm {
	uint32_t vmid;
	uint32_t vcpu_nr;
	uint32_t mmu_on;
	uint32_t index;
	int vcpu_pr;
	int bit64;
	uint32_t vcpu_affinity[CONFIG_VM_MAX_VCPU];
	unsigned long entry_point;
	unsigned long setup_data;
	char name[MINOS_VM_NAME_SIZE];
	char os_type[OS_TYPE_SIZE];
	struct vcpu *vcpus[CONFIG_VM_MAX_VCPU];
	struct mm_struct mm;
	struct os *os;
	struct list_head vm_list;

	unsigned long time_offset;

	/*
	 * each vm may have its own stage2 memory map
	 * to control the memory access
	 */
} __align(sizeof(unsigned long));

#define for_each_vm(vm)	\
	list_for_each_entry(vm, &vm_list, vm_list)

struct vm *get_vm(uint32_t vmid);

int vms_init(void);
void vm_mm_struct_init(struct vm *vm);
int vm_memory_init(struct vm *vm);

#endif
