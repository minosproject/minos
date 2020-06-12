#ifndef __MINOS_PREEMPT_H__
#define __MINOS_PREEMPT_H__

#include <asm/arch.h>

static inline int preempt_allowed(void)
{
	return !current_task_info()->preempt_count;
}

static void inline preempt_enable(void)
{
	current_task_info()->preempt_count--;
}

static void inline preempt_disable(void)
{
	current_task_info()->preempt_count++;
}

#endif
