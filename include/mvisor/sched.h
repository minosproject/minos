/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MVISOR_SCHED_H_
#define _MVISOR_SCHED_H_

#include <mvisor/vcpu.h>
#include <mvisor/percpu.h>
#include <mvisor/list.h>
#include <mvisor/timer.h>

#define PCPU_AFFINITY_FAIL	(0xffff)

DECLARE_PER_CPU(struct vcpu *, current_vcpu);
DECLARE_PER_CPU(struct pcpu *, pcpu);

#define current_vcpu()	get_cpu_var(current_vcpu)

typedef enum _vcpu_state_t {
	VCPU_STATE_ONLINE 	= 0x0001,
	VCPU_STATE_RUNNING 	= 0x0002,
	VCPU_STATE_SLEEP 	= 0x0004,
	VCPU_STATE_OFFLINE  	= 0x0008,
	VCPU_STATE_ERROR 	= 0xffff,
} vcpu_state_t;

typedef enum _pcpu_state_t {
	PCPU_STATE_RUNNING	= 0x0,
	PCPU_STATE_IDLE,
	PCPU_STATE_OFFLINE,
} pcpu_state_t;

struct pcpu {
	uint32_t pcpu_id;
	int need_resched;
	int state;
	struct timer_list sched_timer;
	struct list_head vcpu_list;
	struct list_head ready_list;
};

static vcpu_state_t inline get_vcpu_state(struct vcpu *vcpu)
{
	return vcpu->state;
}

static void inline set_vcpu_state(struct vcpu *vcpu, vcpu_state_t state)
{
	vcpu->state = state;
}

void mvisor_pcpus_init(void);
void sched(void);
void sched_vcpu(struct vcpu *vcpu, int reason);
uint32_t pcpu_affinity(struct vcpu *vcpu, uint32_t affinity);
int vcpu_sched_init(struct vcpu *vcpu);
void vcpu_idle(struct vcpu *vcpu);
void vcpu_online(struct vcpu *vcpu);
void vcpu_offline(struct vcpu *vcpu);
int vcpu_power_on(struct vcpu *caller, int cpuid,
		unsigned long entry, unsigned long unsed);
#endif
