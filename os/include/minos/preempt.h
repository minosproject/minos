#ifndef __MINOS_PREEMPT_H__
#define __MINOS_PREEMPT_H__

#include <minos/percpu.h>
#include <minos/atomic.h>
#include <minos/print.h>
#include <asm/arch.h>
#include <minos/task_def.h>

extern prio_t os_highest_rdy[NR_CPUS];
extern prio_t os_prio_cur[NR_CPUS];

DECLARE_PER_CPU(int, __need_resched);
DECLARE_PER_CPU(int, __int_nesting);
DECLARE_PER_CPU(int, __os_running);

static inline struct task *get_current_task(void)
{
	return current_task_info()->task;
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
	return !current_task_info()->preempt_count;
}

static void inline preempt_enable(void)
{
	current_task_info()->preempt_count--;
	wmb();

#if 0
	if (preempt_allowed() && need_resched()) {
		if ((!int_nesting()) && os_is_running()) {
			clear_need_resched();
			sched();
		}
	}
#endif
}

static void inline preempt_disable(void)
{
	current_task_info()->preempt_count++;
	wmb();
}

#endif
