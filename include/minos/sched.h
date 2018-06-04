/*
 * created by Le MIn 2017/12/09
 */

#ifndef _MINOS_SCHED_H_
#define _MINOS_SCHED_H_

#include <virt/vcpu.h>
#include <minos/percpu.h>
#include <minos/list.h>
#include <minos/timer.h>
#include <minos/task.h>
#include <minos/sched_class.h>

#define PCPU_AFFINITY_FAIL	(0xffff)

DECLARE_PER_CPU(struct pcpu *, pcpu);
DECLARE_PER_CPU(struct task *, percpu_current_task);
DECLARE_PER_CPU(struct task *, percpu_next_task);

#define current_task		get_cpu_var(percpu_current_task)
#define next_task		get_cpu_var(percpu_next_task)

#define current_vcpu		(struct vcpu *)current_task->pdata
#define get_task_state(task)	task->state

typedef enum _pcpu_state_t {
	PCPU_STATE_RUNNING	= 0x0,
	PCPU_STATE_IDLE,
	PCPU_STATE_OFFLINE,
} pcpu_state_t;

struct pcpu {
	uint32_t pcpu_id;
	int need_resched;
	int state;
	spinlock_t lock;

	struct sched_class *sched_class;
	void *sched_data;

	struct list_head task_list;
	struct timer_list sched_timer;
};

#define pcpu_to_sched_data(pcpu)	(pcpu->sched_data)

void pcpus_init(void);
void sched(void);
int pcpu_add_task(int cpu, struct task *task);
void set_task_ready(struct task *task);
void set_task_suspend(struct task *task);
void sched_task(struct task *task, int reason);
int sched_init(void);
int local_sched_init(void);
void sched_new(void);
void pcpu_resched(int pcpu_id);

#endif
