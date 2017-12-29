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
	char name[VMM_VM_NAME_SIZE];
	struct vmm_vcpu *vcpus[CONFIG_VM_MAX_VCPU];
	struct list_head mem_list;
	/*
	 * each vm may have its own stage2 memory map
	 * to control the memory access
	 */
	phy_addr_t ttb2_addr;
	uint64_t tcr_el2;
} vm_t __attribute__((__aligned__ (8)));

#endif
