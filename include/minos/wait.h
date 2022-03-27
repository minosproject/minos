#ifndef __MINOS_WAIT_H__
#define __MINOS_WAIT_H__

#include <minos/task.h>
#include <minos/sched.h>
#include <minos/event.h>

/*
 * -ETIMEOUT
 * -EABORT
 * 0
 */
#define wait_condition(lock, mode, to, condition)		\
({								\
	__label__ __out;					\
	long __ret = 0;						\
	unsigned long flags;					\
	do {							\
		if (condition)					\
			break;					\
								\
		if (is_task_need_stop(current)) {		\
			__ret = -EPERM;				\
			break;					\
		}						\
								\
		spin_lock_irqsave(&lock, flags);		\
		if (condition) {				\
			spin_unlock_irqrestore(&lock, flags);	\
			break;					\
		}						\
		if (is_task_need_stop(current)) {		\
			__ret = -EPERM;				\
			spin_unlock_irqrestore(&lock, flags);	\
			break;					\
		}						\
		__event_task_wait(0, mode, to);			\
		spin_unlock_irqrestore(&lock, flags);		\
								\
		sched();					\
								\
		__ret = task->pend_stat;			\
		task->pend_stat = TASK_STAT_PEND_OK;		\
	} while (0);						\
__out: __ret;							\
})

#endif

