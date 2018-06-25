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

#define VCPU_TASK_DEFAULT_STACK_SIZE	(SIZE_4K * 2)

#define VCPU_MAX_LOCAL_IRQS	(32)
#define CONFIG_VCPU_MAX_ACTIVE_IRQS	(16)

#define VCPU_STAT_READY		(TASK_STAT_READY)
#define VCPU_STAT_RUNNING	(TASK_STAT_RUNNING)
#define VCPU_STAT_SUSPEND	(TASK_STAT_SUSPEND)
#define VCPU_STAT_IDLE		(TASK_STAT_IDLE)

struct virq {
	uint32_t h_intno;
	uint32_t v_intno;
	uint8_t hw;
	uint8_t state;
	uint16_t id;
	uint16_t pr;
	struct list_head list;
};

struct virq_struct {
	uint32_t active_count;
	uint32_t pending_hirq;
	uint32_t pending_virq;
	spinlock_t lock;
	struct list_head pending_list;
	DECLARE_BITMAP(irq_bitmap, CONFIG_VCPU_MAX_ACTIVE_IRQS);
	DECLARE_BITMAP(local_irq_mask, VCPU_MAX_LOCAL_IRQS);
	struct virq virqs[CONFIG_VCPU_MAX_ACTIVE_IRQS];
};

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
#define vcpu_state(vcpu)	(vcpu->task->state)

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

static void inline vcpu_need_resched(struct vcpu *vcpu)
{
	task_need_resched(vcpu_to_task(vcpu));
}

struct vm *get_vm_by_id(uint32_t vmid);
int arch_vm_init(struct vm *vm);
int create_vms(void);
void boot_vms(void);
void sched_vcpu(struct vcpu *vcpu, int reason);
struct vcpu *get_vcpu_in_vm(struct vm *vm, uint32_t vcpu_id);
struct vcpu *get_vcpu_by_id(uint32_t vmid, uint32_t vcpu_id);

void vcpu_idle(void);
int vcpu_suspend(gp_regs *c, uint32_t state, unsigned long entry);
void vcpu_online(struct vcpu *vcpu);
void vcpu_offline(struct vcpu *vcpu);
int vcpu_power_on(struct vcpu *caller, int cpuid,
		unsigned long entry, unsigned long unsed);
#endif
