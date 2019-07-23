#ifndef __MINOS_PREEMPT_H__
#define __MINOS_PREEMPT_H__

#include <minos/percpu.h>
#include <minos/atomic.h>

extern void sched(void);

DECLARE_PER_CPU(int, __preempt);
DECLARE_PER_CPU(int, __need_resched);
DECLARE_PER_CPU(int, __int_nesting);
DECLARE_PER_CPU(int, __os_running);

#define __preempt_disable()	get_cpu_var(__preempt)++
#define __preempt_enable()	get_cpu_var(__preempt)--
#define preempt_allowed()	(!get_cpu_var(__preempt))

static inline void set_need_resched(void)
{
	get_cpu_var(__need_resched) = 1;
	dsb();
}

static inline void clear_need_resched(void)
{
	get_cpu_var(__need_resched) = 0;
	dsb();
}

static inline int need_resched(void)
{
	return  get_cpu_var(__need_resched);
}

static inline void dec_need_resched(void)
{
	get_cpu_var(__need_resched)--;
	dsb();
}

static inline void inc_need_resched(void)
{
	get_cpu_var(__need_resched)++;
	dsb();
}

static inline void inc_int_nesting(void)
{
	get_cpu_var(__int_nesting)++;
	dsb();
}

static inline void dec_int_nesting(void)
{
	get_cpu_var(__int_nesting)--;
	dsb();
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
	dsb();
}

static void inline preempt_enable(void)
{
	__preempt_enable();
	dsb();

	if (preempt_allowed()) {
		if ((!int_nesting()) && need_resched() && os_is_running()) {
			clear_need_resched();
			sched();
		}
	}
}

static void inline preempt_disable(void)
{
	__preempt_disable();
	dsb();
}

#endif
