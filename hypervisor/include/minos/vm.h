/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MINOS_VM_H_
#define _MINOS_VM_H_

#include <minos/types.h>
#include <minos/list.h>
#include <config/config.h>
#include <minos/vmm.h>
#include <minos/errno.h>

#define MINOS_VM_NAME_SIZE	32
#define OS_TYPE_SIZE		32

#define VMID_HOST	(65535)
#define VMID_INVALID	(-1)

struct vcpu;
struct os;
struct vmtag;

extern struct list_head vm_list;

struct vm_info {
	int8_t name[32];
	int8_t os_type[32];
	int32_t nr_vcpus;
	int32_t bit64;
	uint64_t mem_size;
	uint64_t mem_start;
	uint64_t entry;
	uint64_t setup_data;
};

struct vm {
	int vmid;
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
	struct vcpu **vcpus;
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

struct vm *get_vm_by_id(uint32_t vmid);

void vm_mm_struct_init(struct vm *vm);

int vm_server_init(void);
struct vm *create_dynamic_vm(struct vmtag *vme);
int create_new_vm(struct vm_info *info);
void destroy_vm(struct vm *vm);
int vm_power_up(int vmid);

static inline int is_32bit_vm(struct vm *vm)
{
	return (!vm->bit64);
}

static inline int
create_vm_mmap(int vmid,  unsigned long offset, unsigned long size)
{
	struct vm *vm = get_vm_by_id(vmid);

	if (!vm)
		return -ENOENT;

	return vm_mmap(vm, offset, size);
}

static inline void destroy_vm_mmap(int vmid)
{
	struct vm *vm = get_vm_by_id(vmid);

	if (!vm)
		return;

	vm_unmmap(vm);
}

#endif
