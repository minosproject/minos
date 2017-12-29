#ifndef _MVISOR_VM_CONFIG_H_
#define _MVISOR_VM_CONFIG_H_

#include <core/types.h>

struct vmm_memory_region {
	phy_addr_t mem_base;
	phy_addr_t mem_end;
	int type;
	uint32_t vmid;
	char *name;
};

typedef struct vmm_vm_entry {
	uint32_t vmid;
	char *name;
	uint64_t entry_point;
	uint32_t nr_vcpu;
	uint32_t vcpu_affinity[CONFIG_VM_MAX_VCPU];
	void *boot_vm;
} vm_entry_t __attribute__((__aligned__ (8)));

uint32_t get_mem_config_size(void);
void *get_mem_config_data(void);

#define __vmm_vm__	__attribute__((section(".__vmm_vm")))

#define register_vm_entry(vmm_data) \
	static struct vmm_vm_entry *__vmm_vm_##vmm_data __vmm_vm__ = &vmm_data

#endif
