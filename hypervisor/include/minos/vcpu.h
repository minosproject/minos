/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MINOS_VCPU_H_
#define _MINOS_VCPU_H_

#include <minos/types.h>
#include <config/config.h>
#include <minos/list.h>
#include <minos/vm.h>
#include <minos/spinlock.h>

#define VCPU_VCPU_DEFAULT_STACK_SIZE	(SIZE_4K * 2)

#define VCPU_MAX_LOCAL_IRQS		(32)
#define CONFIG_VCPU_MAX_ACTIVE_IRQS	(16)

#define VCPU_NAME_SIZE		(64)

#define VCPU_STAT_READY		(0x0000u)
#define VCPU_STAT_RUNNING	(0x0001u)
#define VCPU_STAT_SEM		(0x0002u)
#define VCPU_STAT_MBOX		(0x0004u)
#define VCPU_STAT_Q		(0x0008u)
#define VCPU_STAT_SUSPEND	(0x0010u)
#define VCPU_STAT_MUTEX		(0x0020u)
#define VCPU_STAT_FLAG		(0x0040u)
#define VCPU_STAT_MULTI		(0x0080u)
#define VCPU_STAT_IDLE		(0x0100u)

#define VCPU_STAT_PEND_OK	(0x0u)
#define VCPU_STAT_PEND_TO	(0x01u)
#define VCPU_STAT_PEND_ABORT	(0x02u)

#define VCPU_TYPE_NORMAL	(0x00u)
#define VCPU_TYPE_VCPU		(0x01u)

#define VCPU_FLAG_NONE		(0x0)
#define VCPU_FLAG_VCPU		(1ul << 1)

#define VCPU_DEFAULT_STACK_SIZE	(SIZE_4K * 2)

#define VCPU_IDLE_PR		(511)

struct virq_struct;

struct vcpu {
	void *stack_base;
	void *stack_origin;
	uint32_t stack_size;
	uint32_t vcpu_id;
	struct vm *vm;
	struct vcpu *next;

	/*
	 * member to record the irq list which the
	 * vcpu is handling now
	 */
	struct virq_struct *virq_struct;
	volatile int state;

	uint32_t affinity;
	uint8_t is_idle;
	uint8_t resched;

	struct list_head list;
	char name[VCPU_NAME_SIZE];
	void *sched_data;

	void **vmodule_context;
	void *arch_data;
} __align_cache_line;

#define VCPU_SCHED_REASON_HIRQ	0x0
#define VCPU_SCHED_REASON_VIRQ	0x1

static int inline get_vcpu_id(struct vcpu *vcpu)
{
	return vcpu->vcpu_id;
}

static int inline get_vmid(struct vcpu *vcpu)
{
	return (vcpu->vm->vmid);
}

static int inline vcpu_affinity(struct vcpu *vcpu)
{
	return vcpu->affinity;
}

struct vm *get_vm_by_id(uint32_t vmid);
int arch_vm_init(struct vm *vm);
int create_vms(void);
void boot_vms(void);
struct vcpu *get_vcpu_in_vm(struct vm *vm, uint32_t vcpu_id);
struct vcpu *get_vcpu_by_id(uint32_t vmid, uint32_t vcpu_id);

struct vcpu *create_idle_vcpu(void);
int vm_vcpus_init(struct vm *vm);

void vcpu_idle(void);
int vcpu_suspend(gp_regs *c, uint32_t state, unsigned long entry);
void vcpu_online(struct vcpu *vcpu);
void vcpu_offline(struct vcpu *vcpu);
int vcpu_power_on(struct vcpu *caller, int cpuid,
		unsigned long entry, unsigned long unsed);
#endif
