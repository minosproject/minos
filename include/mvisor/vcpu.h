/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MVISOR_VCPU_H_
#define _MVISOR_VCPU_H_

#include <mvisor/types.h>
#include <config/config.h>
#include <mvisor/list.h>
#include <mvisor/vm.h>

#include <asm/asm_vcpu.h>

typedef enum _vcpu_state_t {
	VCPU_STATE_READY 	= 0x0001,
	VCPU_STATE_RUNNING 	= 0x0002,
	VCPU_STATE_SLEEP 	= 0x0004,
	VCPU_STATE_STOP  	= 0x0008,
	VCPU_STATE_ERROR 	= 0xffff,
} vcpu_state_t;

typedef struct vmm_vcpu {
	pt_regs regs;
	uint32_t vcpu_id;
	vcpu_state_t state;
	vm_t *vm_belong_to;
	phy_addr_t entry_point;
	uint32_t pcpu_affinity;
	uint32_t status;
	struct list_head pcpu_list;
	void **module_context;
	void *arch_data;
} vcpu_t __attribute__ ((__aligned__ (sizeof(unsigned long))));

static vcpu_state_t inline vmm_get_vcpu_state(vcpu_t *vcpu)
{
	return vcpu->state;
}

static void inline vmm_set_vcpu_state(vcpu_t *vcpu, vcpu_state_t state)
{
	vcpu->state = state;
}

static uint32_t inline vmm_get_vcpu_id(vcpu_t *vcpu)
{
	return vcpu->vcpu_id;
}

static uint32_t inline vmm_get_vm_id(vcpu_t *vcpu)
{
	return (vcpu->vm_belong_to->vmid);
}

static uint32_t inline vmm_get_pcpu_id(vcpu_t *vcpu)
{
	return vcpu->pcpu_affinity;
}

vcpu_t *vmm_get_vcpu(uint32_t vmid, uint32_t vcpu_id);

int arch_vm_init(vm_t *vm);

#endif
