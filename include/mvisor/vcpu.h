/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MVISOR_VCPU_H_
#define _MVISOR_VCPU_H_

#include <mvisor/types.h>
#include <config/config.h>
#include <mvisor/list.h>
#include <mvisor/vm.h>
#include <mvisor/spinlock.h>
#include <asm/asm_vcpu.h>
#include <mvisor/irq.h>

struct vcpu {
	vcpu_regs regs;
	uint32_t vcpu_id;
	struct vm *vm;
	unsigned long entry_point;
	uint32_t pcpu_affinity;
	struct list_head pcpu_list;

	/*
	 * member to record the irq list which the
	 * vcpu is handling now
	 */
	struct irq_struct irq_struct;

	/*
	 * below members is used to sched
	 */
	int state;
	struct list_head state_list;
	unsigned long run_time;
	unsigned long run_start;

	void **module_context;
	void *arch_data;
} __align(sizeof(unsigned long));

static uint32_t inline get_vcpu_id(struct vcpu *vcpu)
{
	return vcpu->vcpu_id;
}

static uint32_t inline get_vmid(struct vcpu *vcpu)
{
	return (vcpu->vm->vmid);
}

static uint32_t inline get_pcpu_id(struct vcpu *vcpu)
{
	return vcpu->pcpu_affinity;
}

struct vcpu *get_vcpu_by_id(uint32_t vmid, uint32_t vcpu_id);

struct vm *get_vm_by_id(uint32_t vmid);

int arch_vm_init(struct vm *vm);

int mvisor_create_vms(void);

struct vcpu *get_vcpu_in_vm(struct vm *vm, uint32_t vcpu_id);

#endif
