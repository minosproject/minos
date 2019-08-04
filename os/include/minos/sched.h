#ifndef _MINOS_SCHED_H_
#define _MINOS_SCHED_H_

#include <minos/percpu.h>
#include <minos/list.h>
#include <minos/timer.h>
#include <minos/atomic.h>
#include <minos/task.h>

DECLARE_PER_CPU(struct pcpu *, pcpu);

extern struct task *__current_tasks[NR_CPUS];
extern struct task *__next_tasks[NR_CPUS];

extern prio_t os_highest_rdy[NR_CPUS];
extern prio_t os_prio_cur[NR_CPUS];

typedef enum _pcpu_state_t {
	PCPU_STATE_RUNNING	= 0x0,
	PCPU_STATE_IDLE,
	PCPU_STATE_OFFLINE,
} pcpu_state_t;

struct pcpu {
	uint32_t pcpu_id;
	volatile int state;

	uint32_t nr_pcpu_task;

	/*
	 * the below two list member can be accessed
	 * by all cpus in the system, when access this
	 * it need to using lock to avoid race condition
	 */
	struct list_head task_list;
	struct list_head new_list;
	spinlock_t lock;

	/*
	 * link to the task for the pcpu and the
	 * task which is ready and which is sleep, these
	 * two list can only be accessed by the cpu
	 * which the task is affinitied.
	 */
	struct list_head ready_list;
	struct list_head sleep_list;

	struct task *idle_task;

	uint64_t resched_ipi_send;
	uint64_t resched_ipi_down;
};

void pcpus_init(void);
void sched(void);
int sched_init(void);
int local_sched_init(void);
void pcpu_resched(int pcpu_id);
int sched_can_idle(struct pcpu *pcpu);
void set_task_ready(struct task *task);
void set_task_suspend(uint32_t delay);
void set_task_sleep(struct task *task);
struct task *get_highest_task(uint8_t group, prio_t *ready);
void irq_enter(gp_regs *regs);
void irq_exit(gp_regs *regs);

static inline struct task *get_current_task(void)
{
	struct task *task;

	task = __current_tasks[smp_processor_id()];
	rmb();

	return task;
}

static inline struct task *get_next_task(void)
{
	struct task *task;

	task = __next_tasks[smp_processor_id()];
	rmb();

	return task;
}

static inline void set_current_task(struct task *task)
{
	__current_tasks[smp_processor_id()] = task;
	wmb();
}

static inline void set_next_task(struct task *task)
{
	__next_tasks[smp_processor_id()] = task;
	wmb();
}

static inline void set_current_prio(prio_t prio)
{
	os_prio_cur[smp_processor_id()] = prio;
}

static inline void set_next_prio(prio_t prio)
{
	os_highest_rdy[smp_processor_id()] = prio;
}

static inline void pcpu_need_resched(void)
{
	inc_need_resched();
}

#endif
