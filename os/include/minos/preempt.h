#ifndef __MINOS_PREEMPT_H__
#define __MINOS_PREEMPT_H__

#include <minos/percpu.h>
#include <minos/atomic.h>

extern void sched(void);

DECLARE_PER_CPU(int, __preempt);
DECLARE_PER_CPU(int, __need_resched);

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

static void inline preempt_disable(void)
{
	__preempt_disable();
}

static void inline preempt_enable(void)
{
	__preempt_enable();

	if (need_resched()) {
		clear_need_resched();
		sched();
	}
}

#endif
