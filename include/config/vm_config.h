/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MVISOR_VM_CONFIG_H_
#define _MVISOR_VM_CONFIG_H_

#include <core/vcpu.h>

struct vmm_vm_entry {
	char *name;
	uint64_t ram_base;
	uint64_t ram_size;
	uint32_t nr_vcpu;
	uint32_t vcpu_affinity[VM_MAX_VCPU_NR];
	int (*boot_vm)(uint64_t ram_base, uint64_t ram_size,
			struct vmm_vcpu_context *c, uint32_t vcpu_id);
};

#define __vmm_vm__	__attribute__((section(".__vmm_vm")))

#define register_vm_entry(vmm_data) \
	static struct vmm_vm_entry *__vmm_vm_##vmm_data = &vmm_data

#endif
