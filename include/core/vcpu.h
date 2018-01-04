/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MVISOR_VCPU_H_
#define _MVISOR_VCPU_H_

#include <core/types.h>
#include <config/mvisor_config.h>
#include <core/list.h>
#include <core/vm.h>

typedef enum _vcpu_state_t {
	VCPU_STATE_READY 	= 0x0001,
	VCPU_STATE_RUNNING 	= 0x0002,
	VCPU_STATE_SLEEP 	= 0x0004,
	VCPU_STATE_STOP  	= 0x0008,
	VCPU_STATE_ERROR 	= 0xffff,
} vcpu_state_t;

#ifdef CONFIG_ARM_AARCH64

typedef struct vmm_vcpu_context {
	uint64_t x0;
	uint64_t x1;
	uint64_t x2;
	uint64_t x3;
	uint64_t x4;
	uint64_t x5;
	uint64_t x6;
	uint64_t x7;
	uint64_t x8;
	uint64_t x9;
	uint64_t x10;
	uint64_t x11;
	uint64_t x12;
	uint64_t x13;
	uint64_t x14;
	uint64_t x15;
	uint64_t x16;
	uint64_t x17;
	uint64_t x18;
	uint64_t x19;
	uint64_t x20;
	uint64_t x21;
	uint64_t x22;
	uint64_t x23;
	uint64_t x24;
	uint64_t x25;
	uint64_t x26;
	uint64_t x27;
	uint64_t x28;
	uint64_t x29;
	uint64_t x30_lr;
	uint64_t sp_el1;
	uint64_t elr_el2;
	uint64_t vbar_el1;
	uint64_t spsr_el2;
	uint64_t nzcv;
	uint64_t esr_el1;
	uint64_t vmpidr;
	uint64_t sctlr_el1;
	uint64_t ttbr0_el1;
	uint64_t ttbr1_el1;
	uint64_t vttbr_el2;
	uint64_t vtcr_el2;
	uint64_t hcr_el2;
} vcpu_context_t __attribute__ ((__aligned__ (sizeof(unsigned long))));

#else

typedef vmm_vcpu_context {

} vcpu_context_t ;

#endif

typedef struct vmm_vcpu {
	vcpu_context_t context;
	uint32_t vcpu_id;
	vcpu_state_t state;
	vm_t *vm_belong_to;
	phy_addr_t entry_point;
	uint32_t pcpu_affinity;
	uint32_t status;
	struct list_head pcpu_list;
} vcpu_t __attribute__ ((__aligned__ (sizeof(unsigned long))));


vcpu_t *create_vcpu(vm_t *vm, int index, boot_vm_t func,
		uint32_t affinity, phy_addr_t entry_point);

static vcpu_state_t inline get_vcpu_state(vcpu_t *vcpu)
{
	return vcpu->state;
}

static void inline set_vcpu_state(vcpu_t *vcpu, vcpu_state_t state)
{
	vcpu->state = state;
}

static uint32_t inline get_vcpu_id(vcpu_t *vcpu)
{
	return vcpu->vcpu_id;
}

static uint32_t inline get_vm_id(vcpu_t *vcpu)
{
	return (vcpu->vm_belong_to->vmid);
}

static uint32_t inline get_pcpu_id(vcpu_t *vcpu)
{
	return vcpu->pcpu_affinity;
}

#endif
