#ifndef _MINOS_SCHED_H_
#define _MINOS_SCHED_H_

#include <minos/timer.h>
#include <minos/atomic.h>
#include <minos/task.h>
#include <minos/current.h>

DECLARE_PER_CPU(struct pcpu *, pcpu);

struct process;

void pcpus_init(void);
void sched(void);
void cond_resched(void);
int sched_init(void);
int local_sched_init(void);
void pcpu_resched(int pcpu_id);
void pcpu_irqwork(int pcpu_id);
void task_sleep(uint32_t ms);
void irq_enter(gp_regs *regs);
void irq_exit(gp_regs *regs);
void cpus_resched(void);
int task_ready(struct task *task, int preempt);

int __wake_up(struct task *task, long pend_state, int type, void *data);

static inline int wake_up(struct task *task)
{
	return __wake_up(task, TASK_STAT_PEND_OK, 0, NULL);
}

static inline int wake_up_timeout(struct task *task)
{
	return __wake_up(task, TASK_STAT_PEND_TO, 0, NULL);
}

static inline int wake_up_abort(struct task *task)
{
	return __wake_up(task, TASK_STAT_PEND_ABORT, 0, NULL);
}

/*
 * set current running task's state do not need to obtain
 * a lock, when need to wakeup the task, below state the state
 * can be changed:
 * 1 - running -> wait_event
 * 2 - wait_event -> running (waked up by event)
 * 3 - new -> running
 * 4 - running -> stopped
 */
#define set_current_state(state, to) 		\
	do {			 		\
		current->stat = (state); 	\
		current->delay = (to);		\
		smp_mb();			\
	} while (0)

#endif
