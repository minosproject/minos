/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MVISOR_VM_H_
#define _MVISOR_VM_H_

#include <core/types.h>
#include <core/list.h>
#include <config/mvisor_config.h>
#include <core/mmu.h>

#define VMM_VM_NAME_SIZE	32

struct vmm_vcpu;
struct vmm_vcpu_context;


typedef int (*boot_vm_t)(void *arg);

typedef struct vmm_vm {
	uint32_t vmid;
	uint32_t vcpu_nr;
	uint32_t mmu_on;
	uint32_t index;
	uint32_t vcpu_affinity[CONFIG_VM_MAX_VCPU];
	phy_addr_t entry_point;
	char name[VMM_VM_NAME_SIZE];
	struct vmm_vcpu *vcpus[CONFIG_VM_MAX_VCPU];
	struct list_head mem_list;
	/*
	 * each vm may have its own stage2 memory map
	 * to control the memory access
	 */
	phy_addr_t vttbr_el2_addr;
	uint64_t vtcr_el2;
	uint64_t hcr_el2;
	boot_vm_t boot_vm;
} vm_t __attribute__((__aligned__ (8)));

#endif
