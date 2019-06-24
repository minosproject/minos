#ifndef _MINOS_SCHED_H_
#define _MINOS_SCHED_H_

#include <minos/percpu.h>
#include <minos/list.h>
#include <minos/timer.h>
#include <minos/atomic.h>
#include <minos/task.h>

DECLARE_PER_CPU(struct pcpu *, pcpu);
DECLARE_PER_CPU(struct task *, percpu_current_task);
DECLARE_PER_CPU(struct task *, percpu_next_task);
DECLARE_PER_CPU(atomic_t, __need_resched);
DECLARE_PER_CPU(atomic_t, __preempt);
DECLARE_PER_CPU(atomic_t, __int_nesting);
DECLARE_PER_CPU(int, __os_running);

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
void sched_new(void);
void pcpu_resched(int pcpu_id);
int sched_can_idle(struct pcpu *pcpu);
void set_task_ready(struct task *task);
void set_task_suspend(struct task *task, uint32_t delay);
void set_task_sleep(struct task *task);
struct task *get_highest_task(uint8_t group, prio_t *ready);
void irq_enter(gp_regs *regs);
void irq_exit(gp_regs *regs);
void pcpu_need_resched(void);

static inline struct task *get_current_task(void)
{
	return get_cpu_var(percpu_current_task);
}

static inline struct task *get_next_task(void)
{
	return get_cpu_var(percpu_next_task);
}

static inline void set_current_task(struct task *task)
{
	get_cpu_var(percpu_current_task) = task;
}

static inline void set_next_task(struct task *task)
{
	get_cpu_var(percpu_next_task) = task;
}

static inline void set_need_resched(void)
{
	atomic_set(&get_cpu_var(__need_resched), 1);
}

static inline void clear_need_resched(void)
{
	atomic_set(&get_cpu_var(__need_resched), 0);
}

static inline int need_resched(void)
{
	return atomic_read(&get_cpu_var(__need_resched));
}

static inline void inc_int_nesting(void)
{
	atomic_inc(&get_cpu_var(__int_nesting));
}

static inline void dec_int_nesting(void)
{
	atomic_dec(&get_cpu_var(__int_nesting));
}

static inline int int_nesting(void)
{
	return atomic_read(&get_cpu_var(__int_nesting));
}

static inline int os_is_running(void)
{
	return get_cpu_var(__os_running);
}

static inline void set_os_running(void)
{
	get_cpu_var(__os_running) = 1;
}

static inline void set_current_prio(prio_t prio)
{
	os_prio_cur[smp_processor_id()] = prio;
}

static inline void set_next_prio(prio_t prio)
{
	os_highest_rdy[smp_processor_id()] = prio;
}

#endif
