#ifndef _MVISOR_VM_CONFIG_H_
#define _MVISOR_VM_CONFIG_H_

#include <mvisor/types.h>
#include <config/config.h>

typedef struct mvisor_vm_entry {
	uint32_t vmid;
	char *name;
	uint64_t entry_point;
	uint32_t nr_vcpu;
	uint32_t vcpu_affinity[CONFIG_VM_MAX_VCPU];
	uint32_t mmu_on;
	void *boot_vcpu;
} vm_entry_t __align(sizeof(unsigned long));

uint32_t get_mem_config_size(void);
void *get_mem_config_data(void);
void *get_memory_regions(void);
int get_irq_config_size(void);
void *get_irq_config_table(void);

#define __mvisor_vm__	__section(".__mvisor_vm")

#define register_vm_entry(mvisor_data) \
	static struct mvisor_vm_entry *__mvisor_vm_##mvisor_data \
	__used __mvisor_vm__ = &mvisor_data

#endif
