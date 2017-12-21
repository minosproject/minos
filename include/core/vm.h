/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MVISOR_VM_H_
#define _MVISOR_VM_H_

#include <core/types.h>
#include <core/list.h>
#include <config/mvisor_config.h>

#define VMM_VM_NAME_SIZE	32

struct vmm_vcpu;
struct vmm_vcpu_context;

typedef enum _vm_feature_t {
	VM_FEATURE_SWIO		= 0X00000001,
	VM_FEATURE_PTW		= 0X00000002,
	VM_FEATURE_FB		= 0x00000004,
	VM_FEATURE_TRAP_WFI	= 0x00000008,
	VM_FEATURE_TRAP_WFE	= 0x00000010,
	VM_FEATURE_TRAP_TID1	= 0x00000020,
	VM_FEATURE_TRAP_TID2	= 0x00000020,
	VM_FEATURE_TRAP_TID3	= 0x00000040,
	VM_FEATURE_TRAP_SMC	= 0x00000080,
	VM_FEATURE_TRAP_CP	= 0x00000100,
	VM_FEATURE_TRAP_ACTLR	= 0x00000200,
	VM_FEATURE_TRAP_SW	= 0x00000400,
	VM_FEATURE_TRAP_PC	= 0x00000800,
	VM_FEATURE_TRAP_PU	= 0x00001000,
	VM_FEATURE_TRAP_TTLB	= 0x00002000,
	VM_FEATURE_TRAP_TVM	= 0x00004000,
	VM_FEATURE_TRAP_TGE	= 0x00008000,
	VM_FEATURE_TRAP_DCZVA	= 0x00010000,
	VM_FEATURE_HVC_ENABLE	= 0x00020000,
	VM_FEATURE_VM_WIDTH	= 0x00040000,
} vm_feature_t;

typedef	int (*boot_vm_t)(struct vmm_vcpu_context *context);

typedef struct vmm_vm {
	uint32_t vmid;
	uint32_t vcpu_nr;
	char name[VMM_VM_NAME_SIZE];
	struct vmm_vcpu *vcpus[VM_MAX_VCPU];
	phy_addr_t ram_base;
	phy_addr_t ram_size;
} vm_t __attribute__((__aligned__ (8)));

typedef struct vmm_vm_entry {
	char *name;
	uint64_t ram_base;
	uint64_t ram_size;
	uint64_t entry_point;
	uint32_t nr_vcpu;
	uint32_t vcpu_affinity[VM_MAX_VCPU];
	boot_vm_t boot_vm;
} vm_entry_t __attribute__((__aligned__ (8)));

int register_vcpu_context(phy_addr_t *context,
		uint32_t vmid, uint32_t vcpuid);

#define __vmm_vm__	__attribute__((section(".__vmm_vm")))

#define register_vm_entry(vmm_data) \
	static struct vmm_vm_entry *__vmm_vm_##vmm_data __vmm_vm__ = &vmm_data

#endif
