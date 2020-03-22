#ifndef _MINOS_SCHED_H_
#define _MINOS_SCHED_H_

#include <minos/percpu.h>
#include <minos/list.h>
#include <minos/timer.h>
#include <minos/atomic.h>
#include <minos/task.h>
#include <minos/flag.h>

DECLARE_PER_CPU(struct pcpu *, pcpu);

typedef enum _pcpu_state_t {
	PCPU_STATE_RUNNING	= 0x0,
	PCPU_STATE_IDLE,
	PCPU_STATE_OFFLINE,
} pcpu_state_t;

#define PCPU_IDLE_F_TASKS_RELEASE	(1 << 0)

struct pcpu {
	uint32_t pcpu_id;
	volatile int state;

	uint32_t nr_pcpu_task;

	/*
	 * the below two list member can be accessed
	 * by all cpus in the system, when access this
	 * it need to using lock to avoid race condition
	 * when process the below three list, need to get
	 * the lock
	 */
	spinlock_t lock;
	struct list_head task_list;
	struct list_head new_list;
	struct list_head stop_list;

	struct task *running_task;
	struct task *idle_task;

	/*
	 * each pcpu has its local sched list, 8 priority
	 * local_rdy_grp only use [0 - 8], in these 8
	 * priority:
	 * 7 - used for idle task
	 * 6 - used for vcpu task
	 */
	uint8_t local_rdy_grp;
	struct list_head ready_list[8];

	/*
	 * each pcpu will have one kernel task which
	 * will do some maintenance work for the pcpu
	 */
	struct task *kworker;
	struct flag_grp fg;

	/* sched class callback for each pcpu */
	void (*sched)(struct pcpu *pcpu, struct task *cur);
	void (*irq_handler)(struct pcpu *pcpu, struct task *cur);
	void (*switch_out)(struct pcpu *pcpu,
			struct task *cur, struct task *next);
	void (*switch_to)(struct pcpu *pcpu, struct task *cur,
			struct task *next);
};

#define add_task_to_ready_list(pcpu, task)	\
	list_add(&pcpu->ready_list[task->local_prio], &task->stat_list)
#define add_task_to_ready_list_tail(pcpu, task)	\
	list_add_tail(&pcpu->ready_list[task->local_prio], &task->stat_list)

void pcpus_init(void);
void sched(void);
void sched_yield(void);
int sched_init(void);
int local_sched_init(void);
void pcpu_resched(int pcpu_id);
void pcpu_irqwork(int pcpu_id);
int sched_can_idle(struct pcpu *pcpu);
int set_task_ready(struct task *task, int preempt);
void set_task_suspend(uint32_t delay);
int set_task_sleep(struct task *task, uint32_t ms);
struct task *get_highest_task(uint8_t group, uint8_t *ready);
void irq_enter(gp_regs *regs);
void irq_exit(gp_regs *regs);
void sched_task(struct task *task);
void cpus_resched(void);
int select_task_run_cpu(void);

#endif
