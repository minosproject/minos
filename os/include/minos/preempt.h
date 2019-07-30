#ifndef __MINOS_PREEMPT_H__
#define __MINOS_PREEMPT_H__

#include <minos/percpu.h>
#include <minos/atomic.h>

extern void sched(void);

DECLARE_PER_CPU(int, __preempt);
DECLARE_PER_CPU(int, __need_resched);
DECLARE_PER_CPU(int, __int_nesting);
DECLARE_PER_CPU(int, __os_running);

#define __preempt_disable() \
	do { \
		get_cpu_var(__preempt)++; \
	} while (0)

#define __preempt_enable() \
	do { \
		get_cpu_var(__preempt)--; \
	} while (0)

static inline int preempt_allowed(void)
{
	return (!get_cpu_var(__preempt));
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

static void inline preempt_enable(void)
{
	__preempt_enable();

#if 0
	if (preempt_allowed() && need_resched()) {
		if ((!int_nesting()) && os_is_running()) {
			clear_need_resched();
			sched();
		}
	}
#endif
	mb();
}

static void inline preempt_disable(void)
{
	__preempt_disable();
}

#endif
