#ifndef __MINOS_PREEMPT_H__
#define __MINOS_PREEMPT_H__

#include <minos/task_info.h>

extern void cond_resched(void);

static inline int preempt_allowed(void)
{
	return !get_current_task_info()->preempt_count;
}

static inline void preempt_enable(void)
{
	get_current_task_info()->preempt_count--;
	wmb();
	cond_resched();
}

static void inline preempt_disable(void)
{
	get_current_task_info()->preempt_count++;
	wmb();
}

#endif
