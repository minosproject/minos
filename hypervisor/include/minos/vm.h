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

#define VM_MAX_VCPU		CONFIG_NR_CPUS

#define MINOS_VM_NAME_SIZE	32
#define OS_TYPE_SIZE		32

#define VMID_HOST		(65535)
#define VMID_INVALID		(-1)

#define VM_FLAGS_64BIT		(1 << 0)
#define VM_FLAGS_NATIVE		(1 << 1)

#define VM_STAT_OFFLINE		(0)
#define VM_STAT_ONLINE		(1)
#define VM_STAT_SUSPEND		(2)
#define VM_STAT_REBOOT		(3)

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
	uint64_t mmap_base;
};

struct vm {
	int vmid;
	uint32_t vcpu_nr;
	int state;
	unsigned long flags;
	uint8_t vcpu_affinity[VM_MAX_VCPU];
	unsigned long entry_point;
	unsigned long setup_data;
	char name[MINOS_VM_NAME_SIZE];
	char os_type[OS_TYPE_SIZE];
	struct vcpu **vcpus;
	struct list_head vcpu_list;
	struct mm_struct mm;
	struct os *os;
	struct list_head vm_list;

	unsigned long time_offset;

	struct list_head vdev_list;

	uint32_t vspi_nr;
	int virq_same_page;
	struct virq_desc *vspi_desc;
	unsigned long *vspi_map;

	void *vmcs;
	void *hvm_vmcs;
} __align(sizeof(unsigned long));

extern struct vm *vms[CONFIG_MAX_VM];

#define for_each_vm(vm)	\
	list_for_each_entry(vm, &vm_list, vm_list)

#define vm_for_each_vcpu(vm, vcpu)	\
	for (vcpu = vm->vcpus[0]; vcpu != NULL; vcpu = vcpu->next)

void vm_mm_struct_init(struct vm *vm);

struct vm *create_vm(struct vmtag *vme);
int create_new_vm(struct vm_info *info);
void destroy_vm(struct vm *vm);
int vm_power_up(int vmid);
int vm_reset(int vmid, void *args);
int vm_power_off(int vmid, void *arg);

static inline struct vm *get_vm_by_id(uint32_t vmid)
{
	if (vmid >= CONFIG_MAX_VM)
		return NULL;

	return vms[vmid];
}

static inline int vm_is_32bit(struct vm *vm)
{
	return !(vm->flags & VM_FLAGS_64BIT);
}

static inline int vm_is_64bit(struct vm *vm)
{
	return !!(vm->flags & VM_FLAGS_64BIT);
}

static inline int vm_is_hvm(struct vm *vm)
{
	return (vm->vmid == 0);
}

static inline int vm_is_native(struct vm *vm)
{
	return !!(vm->flags & VM_FLAGS_NATIVE);
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
