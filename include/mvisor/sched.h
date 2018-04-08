/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MVISOR_SCHED_H_
#define _MVISOR_SCHED_H_

#include <mvisor/vcpu.h>
#include <mvisor/percpu.h>
#include <mvisor/list.h>

#define PCPU_AFFINITY_FAIL	(0xffff)

DECLARE_PER_CPU(vcpu_t *, current_vcpu);
#define current_vcpu()	get_cpu_var(current_vcpu)

typedef enum _vcpu_state_t {
	VCPU_STATE_READY 	= 0x0001,
	VCPU_STATE_RUNNING 	= 0x0002,
	VCPU_STATE_SLEEP 	= 0x0004,
	VCPU_STATE_STOP  	= 0x0008,
	VCPU_STATE_ERROR 	= 0xffff,
} vcpu_state_t;

typedef struct vmm_pcpu {
	uint32_t pcpu_id;
	int need_resched;
	struct list_head vcpu_list;
	struct list_head ready_list;
} pcpu_t;

static vcpu_state_t inline get_vcpu_state(vcpu_t *vcpu)
{
	return vcpu->state;
}

static void inline set_vcpu_state(vcpu_t *vcpu, vcpu_state_t state)
{
	vcpu->state = state;
}

void vmm_pcpus_init(void);
void sched(void);
void sched_vcpu(vcpu_t *vcpu, int reason);
uint32_t pcpu_affinity(vcpu_t *vcpu, uint32_t affinity);
int vcpu_sched_init(vcpu_t *vcpu);
void vcpu_idle(vcpu_t *vcpu);

#endif
