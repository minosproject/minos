#ifndef __MINOS_TASK_H__
#define __MINOS_TASK_H__

#include <minos/minos.h>
#include <minos/flag.h>
#include <config/config.h>
#include <minos/task_def.h>

#define task_info(task)	((struct task_info *)task->stack_origin)

static int inline task_is_idle(struct task *task)
{
	return (task->prio == OS_PRIO_IDLE);
}

static inline int get_task_pid(struct task *task)
{
	return task->pid;
}

static inline prio_t get_task_prio(struct task *task)
{
	return task->prio;
}

static inline int task_is_realtime(struct task *task)
{
	return (task->prio <= OS_LOWEST_REALTIME_PRIO);
}

static inline int task_is_percpu(struct task *task)
{
	return (task->prio > OS_LOWEST_REALTIME_PRIO);
}

static inline int task_is_pending(struct task *task)
{
	return ((task->stat & TASK_STAT_PEND_ANY) != 
			TASK_STAT_RDY);
}

static inline int task_is_suspend(struct task *task)
{
	return !!(task->stat & TASK_STAT_SUSPEND);
}

static inline int task_is_ready(struct task *task)
{
	return ((task->stat == TASK_STAT_RDY) ||
			(task->stat == TASK_STAT_RUNNING));
}

static inline int task_is_32bit(struct task *task)
{
	return !!(task->flags & TASK_FLAGS_32BIT);
}

static inline int task_is_64bit(struct task *task)
{
	return !(task->flags & TASK_FLAGS_32BIT);
}

static inline int task_is_vcpu(struct task *task)
{
	return (task->flags & TASK_FLAGS_VCPU);
}

static inline void task_set_resched(struct task *task)
{
	struct task_info *tf = (struct task_info *)task->stack_origin;

	tf->flags |= TIF_NEED_RESCHED;
}

static inline void task_clear_resched(struct task *task)
{
	struct task_info *tf = (struct task_info *)task->stack_origin;

	tf->flags &= ~TIF_NEED_RESCHED;
}

static inline int task_need_resched(struct task *task)
{
	struct task_info *tf = (struct task_info *)task->stack_origin;

	return (tf->flags & TIF_NEED_RESCHED);
}

int create_percpu_task(char *name, task_func_t func,
		void *arg, size_t stk_size, unsigned long flags);

int create_realtime_task(char *name, task_func_t func, void *arg,
		prio_t prio, size_t stk_size, unsigned long flags);

int create_vcpu_task(char *name, task_func_t func, void *arg,
		int aff, unsigned long flags);

int create_task(char *name, task_func_t func,
		void *arg, prio_t prio, uint16_t aff,
		size_t stk_size, unsigned long opt);

void release_task(struct task *task);
void do_release_task(struct task *task);
struct task *pid_to_task(int pid);
void os_for_all_task(void (*hdl)(struct task *task));

#define task_lock(task)					\
	do {						\
		if (task_is_realtime(task))		\
			kernel_lock();			\
		else					\
			raw_spin_lock(&task->lock);	\
	} while (0)

#define task_unlock(task)				\
	do {						\
		if (task_is_realtime(task)) 		\
			kernel_unlock();		\
		else					\
			raw_spin_unlock(&task->lock);	\
	} while (0)

#define task_lock_irqsave(task, flags)			\
	do {						\
		if (task_is_realtime(task)) 		\
			kernel_lock_irqsave(flags);	\
		else					\
			spin_lock_irqsave(&task->lock, flags);	\
	} while (0)

#define task_unlock_irqrestore(task, flags)		\
	do {						\
		if (task_is_realtime(task)) 		\
			kernel_unlock_irqrestore(flags);\
		else					\
			spin_unlock_irqrestore(&task->lock, flags);	\
	} while (0)

#endif
