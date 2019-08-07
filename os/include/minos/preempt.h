#ifndef __MINOS_PREEMPT_H__
#define __MINOS_PREEMPT_H__

#include <minos/percpu.h>
#include <minos/atomic.h>
#include <minos/print.h>
#include <minos/smp.h>
#include <minos/task_def.h>

extern struct task *__current_tasks[NR_CPUS];
extern struct task *__next_tasks[NR_CPUS];

extern prio_t os_highest_rdy[NR_CPUS];
extern prio_t os_prio_cur[NR_CPUS];

DECLARE_PER_CPU(int, __need_resched);
DECLARE_PER_CPU(int, __int_nesting);
DECLARE_PER_CPU(int, __os_running);

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



static inline void set_need_resched(void)
{
	get_cpu_var(__need_resched) = 1;
}

static inline void clear_need_resched(void)
{
	get_cpu_var(__need_resched) = 0;
}

static inline int need_resched(void)
{
	return  get_cpu_var(__need_resched);
}

static inline void dec_need_resched(void)
{
	get_cpu_var(__need_resched)--;
}

static inline void inc_need_resched(void)
{
	get_cpu_var(__need_resched)++;
}

static inline void pcpu_need_resched(void)
{
	inc_need_resched();
}

static inline void inc_int_nesting(void)
{
	get_cpu_var(__int_nesting)++;
}

static inline void dec_int_nesting(void)
{
	get_cpu_var(__int_nesting)--;
}

static inline int int_nesting(void)
{
	return get_cpu_var(__int_nesting);
}

static inline int os_is_running(void)
{
	return get_cpu_var(__os_running);
}

static inline void set_os_running(void)
{
	get_cpu_var(__os_running) = 1;
}

static inline int preempt_allowed(void)
{
	struct task *task = get_current_task();

	return (!task->preempt);
}

static void inline preempt_enable(void)
{
	struct task *task = get_current_task();

	task->preempt--;
	wmb();

#if 0
	if (preempt_allowed() && need_resched()) {
		if ((!int_nesting()) && os_is_running()) {
			clear_need_resched();
			sched();
		}
	}
#endif
	pr_info("#### %d\n", task->preempt);
}

static void inline preempt_disable(void)
{
	struct task *task = get_current_task();

	task->preempt++;
	wmb();
	pr_info("#### %d\n", task->preempt);
}

#endif
