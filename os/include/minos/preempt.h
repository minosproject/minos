#ifndef __MINOS_PREEMPT_H__
#define __MINOS_PREEMPT_H__

#include <minos/percpu.h>
#include <minos/atomic.h>
#include <minos/print.h>
#include <asm/arch.h>
#include <minos/bitops.h>
#include <minos/task_def.h>

extern prio_t os_highest_rdy[NR_CPUS];
extern prio_t os_prio_cur[NR_CPUS];

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
	set_bit(TIF_NEED_RESCHED, &current_task_info()->flags);
}

static inline void clear_need_resched(void)
{
	clear_bit(TIF_NEED_RESCHED, &current_task_info()->flags);
}

static inline int need_resched(void)
{
	return  (current_task_info()->flags & __TIF_NEED_RESCHED);
}

static inline int os_is_running(void)
{
	int running = 0;
	unsigned long flags;

	local_irq_save(flags);
	running = get_cpu_var(__os_running);
	local_irq_restore(flags);

	return running;
}

static inline void set_os_running(void)
{
	/*
	 * os running is set before the irq is enable
	 * so do not need to aquire lock or disable the
	 * interrupt here
	 */
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
}

static void inline preempt_disable(void)
{
	current_task_info()->preempt_count++;
	wmb();
}

static void inline might_sleep(void)
{

}

#endif
