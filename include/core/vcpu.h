/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MVISOR_VCPU_H_
#define _MVISOR_VCPU_H_

#include <core/types.h>
#include <config/mvisor_config.h>
#include <core/list.h>

typedef enum _vcpu_state_t {
	VCPU_STATE_READY = 0x0001,
	VCPU_STATE_RUNNING = 0x0002,
	VCPU_STATE_SLEEP = 0x0004,
	VCPU_STATE_STOP  = 0x0008,
	VCPU_STATE_ERROR = 0xffff,
} vcpu_state_t;

struct vmm_vm;

struct vmm_vcpu_context {
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
	uint64_t spsr_el1;
	uint64_t nzcv;
	uint64_t esr_el1;
} __attribute__ ((__aligned__ (8)));

struct vmm_vcpu {
	uint32_t vcpu_id;
	vcpu_state_t state;
	struct vmm_vm *vm_belong_to;
	uint32_t pcpu_affinity;
	struct list_head pcpu_list;
	uint32_t status;
	struct vmm_vcpu_context context;
} __attribute__ ((__aligned__ (8)));

typedef	int (*boot_vm_t)(uint64_t ram_base, uint64_t ram_size,
			struct vmm_vcpu_context *c, uint32_t vcpu_id);

struct vmm_vcpu *create_vcpu(struct vmm_vm *vm,
		int index, boot_vm_t func, uint32_t affinity);

static uint32_t inline get_vcpu_id(struct vmm_vcpu *vcpu)
{
	return vcpu->vcpu_id;
}

static vcpu_state_t inline
get_vcpu_state(struct vmm_vcpu *vcpu)
{
	return vcpu->state;
}

static void inline
set_vcpu_state(struct vmm_vcpu *vcpu, vcpu_state_t state)
{
	vcpu->state = state;
}

#endif
