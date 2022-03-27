#ifndef __MINOS_TASK_H__
#define __MINOS_TASK_H__

#include <minos/minos.h>
#include <minos/flag.h>
#include <config/config.h>
#include <minos/task_def.h>

#define to_task_info(task)	(&(task)->ti)

#ifdef CONFIG_TASK_RUN_TIME
#define TASK_RUN_TIME CONFIG_TASK_RUN_TIME
#else
#define TASK_RUN_TIME 100
#endif

static int inline task_is_idle(struct task *task)
{
	return (task->flags & TASK_FLAGS_IDLE);
}

static inline int get_task_tid(struct task *task)
{
	return task->tid;
}

static inline uint8_t get_task_prio(struct task *task)
{
	return task->prio;
}

static inline int task_is_suspend(struct task *task)
{
	return !!(task->stat & TASK_STAT_WAIT_EVENT);
}

static inline int task_is_running(struct task *task)
{
	return (task->stat == TASK_STAT_RUNNING);
}

static inline int task_is_vcpu(struct task *task)
{
	return (task->flags & TASK_FLAGS_VCPU);
}

static inline int task_is_32bit(struct task *task)
{
	return (task->flags & TASK_FLAGS_32BIT);
}

static inline void task_set_resched(struct task *task)
{
	task->ti.flags |= TIF_NEED_RESCHED;
}

static inline void task_clear_resched(struct task *task)
{
	task->ti.flags &= ~TIF_NEED_RESCHED;
}

static inline int task_need_resched(struct task *task)
{
	return (task->ti.flags & TIF_NEED_RESCHED);
}

static inline void task_need_stop(struct task *task)
{
	set_bit(TIF_NEED_STOP, &task->ti.flags);
	smp_wmb();
}

static inline void task_need_freeze(struct task *task)
{
	set_bit(TIF_NEED_FREEZE, &task->ti.flags);
	smp_wmb();
}

static inline int is_task_need_stop(struct task *task)
{
	return !!(task->ti.flags & (__TIF_NEED_FREEZE | __TIF_NEED_STOP));
}

static inline void set_current_stat(int stat)
{
	current->stat = stat;
	smp_wmb();
}

#define task_stat_pend_ok(status)	\
	((status) == TASK_STAT_PEND_OK)
#define task_stat_pend_timeout(status)	\
	((status) == TASK_STAT_PEND_TO)
#define task_stat_pend_abort(status)	\
	((status) == TASK_STAT_PEND_ABORT)

void do_release_task(struct task *task);

struct task *create_task(char *name,
		task_func_t func,
		size_t stk_size,
		int prio,
		int aff,
		unsigned long opt,
		void *arg);

struct task *create_vcpu_task(char *name, task_func_t func, int aff,
		unsigned long flags, void *vcpu);
#endif
