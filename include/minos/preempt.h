#ifndef __MINOS_PREEMPT_H__
#define __MINOS_PREEMPT_H__

#include <minos/task_info.h>

static inline int preempt_allowed(void)
{
	return !get_current_task_info()->preempt_count;
}

static void inline preempt_enable(void)
{
	get_current_task_info()->preempt_count--;
	wmb();
}

static void inline preempt_disable(void)
{
	get_current_task_info()->preempt_count++;
	wmb();
}

#endif
