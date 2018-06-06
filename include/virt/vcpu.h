/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MINOS_VCPU_H_
#define _MINOS_VCPU_H_

#include <minos/types.h>
#include <config/config.h>
#include <minos/list.h>
#include <virt/vm.h>
#include <minos/spinlock.h>
#include <minos/task.h>
#include <minos/cpumask.h>
#include <virt/virq.h>

#define VCPU_TASK_DEFAULT_STACK_SIZE	(SIZE_4K * 2)

struct vcpu {
	uint32_t vcpu_id;
	struct vm *vm;
	struct task *task;

	/*
	 * member to record the irq list which the
	 * vcpu is handling now
	 */
	struct virq_struct virq_struct;

	void **vmodule_context;
	void *arch_data;
} __align_cache_line;

#define vcpu_to_task(vcpu)	(vcpu->task)
#define vcpu_affinity(vcpu)	(vcpu->task->affinity)

#define VCPU_SCHED_REASON_HIRQ	0x0
#define VCPU_SCHED_REASON_VIRQ	0x1

static uint32_t inline get_vcpu_id(struct vcpu *vcpu)
{
	return vcpu->vcpu_id;
}

static uint32_t inline get_vmid(struct vcpu *vcpu)
{
	return (vcpu->vm->vmid);
}

struct vm *get_vm_by_id(uint32_t vmid);
int arch_vm_init(struct vm *vm);
int create_vms(void);
void boot_vms(void);
void sched_vcpu(struct vcpu *vcpu, int reason);
struct vcpu *get_vcpu_in_vm(struct vm *vm, uint32_t vcpu_id);
struct vcpu *get_vcpu_by_id(uint32_t vmid, uint32_t vcpu_id);

#endif
