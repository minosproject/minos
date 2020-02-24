/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MVISOR_VM_H_
#define _MVISOR_VM_H_

#include <core/types.h>
#include <core/vcpu.h>
#include <core/list.h>

#define VMM_VM_NAME_SIZE	32

struct vmm_vm {
	uint32_t vmid;
	uint32_t vcpu_nr;
	char name[VMM_VM_NAME_SIZE];
	struct vmm_vcpu *vcpus[VM_MAX_VCPU];
	uint64_t ram_base;
	uint64_t ram_size;
} __attribute__ ((__aligned__ (8)));

struct vmm_vm_entry {
	char *name;
	uint64_t ram_base;
	uint64_t ram_size;
	uint32_t nr_vcpu;
	uint32_t vcpu_affinity[VM_MAX_VCPU];
	int (*boot_vm)(uint64_t ram_base, uint64_t ram_size,
			struct vmm_vcpu_context *c, uint32_t vcpu_id);
} __attribute__ ((__aligned__ (8)));

#define __vmm_vm__	__attribute__((section(".__vmm_vm")))

#define register_vm_entry(vmm_data) \
	static struct vmm_vm_entry *__vmm_vm_##vmm_data = &vmm_data

#endif
