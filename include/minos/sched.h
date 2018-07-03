/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MINOS_SCHED_H_
#define _MINOS_SCHED_H_

#include <minos/vcpu.h>
#include <minos/percpu.h>
#include <minos/list.h>
#include <minos/timer.h>
#include <minos/sched_class.h>

#define PCPU_AFFINITY_FAIL	(0xffff)

DECLARE_PER_CPU(struct pcpu *, pcpu);
DECLARE_PER_CPU(struct vcpu *, percpu_current_vcpu);
DECLARE_PER_CPU(struct vcpu *, percpu_next_vcpu);

DECLARE_PER_CPU(int, need_resched);

#define current_vcpu		get_cpu_var(percpu_current_vcpu)
#define next_vcpu		get_cpu_var(percpu_next_vcpu)

#define get_vcpu_state(vcpu)	vcpu->state
#define need_resched		get_cpu_var(need_resched)

#define SCHED_REASON_IRQ	(0x0)

typedef enum _pcpu_state_t {
	PCPU_STATE_RUNNING	= 0x0,
	PCPU_STATE_IDLE,
	PCPU_STATE_OFFLINE,
} pcpu_state_t;

struct pcpu {
	uint32_t pcpu_id;
	int state;
	spinlock_t lock;

	struct sched_class *sched_class;
	void *sched_data;

	struct list_head vcpu_list;
};

#define pcpu_to_sched_data(pcpu)	(pcpu->sched_data)

void pcpus_init(void);
void sched(void);
int pcpu_add_vcpu(int cpu, struct vcpu *vcpu);
void set_vcpu_state(struct vcpu *vcpu, int state);
void sched_vcpu(struct vcpu *vcpu);
int sched_init(void);
int local_sched_init(void);
void sched_new(void);
void pcpu_resched(int pcpu_id);

static inline void set_vcpu_ready(struct vcpu *vcpu)
{
	set_vcpu_state(vcpu, VCPU_STAT_READY);
}

static inline void set_vcpu_suspend(struct vcpu *vcpu)
{
	set_vcpu_state(vcpu, VCPU_STAT_SUSPEND);
}

#endif
