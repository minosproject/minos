#ifndef __MINOS_TASK_H__
#define __MINOS_TASK_H__

#include <minos/minos.h>
#include <minos/flag.h>
#include <config/config.h>
#include <minos/task_def.h>

extern struct task *os_task_table[OS_NR_TASKS];

#define DEFINE_TASK(nn, f, a, p, af, ss, fl) \
	static const struct task_desc __used \
	task_desc_##f __section(.__task_desc) = { \
		.name = nn,		\
		.func = f,		\
		.arg = a,		\
		.prio = p,		\
		.aff = af,		\
		.stk_size = ss,		\
		.flags = fl		\
	}

#define DEFINE_TASK_PERCPU(nn, f, a, ss, fl) \
	static const struct task_desc __used \
	task_desc_##f __section(.__task_desc) = { \
		.name = nn,		\
		.func = f,		\
		.arg = a,		\
		.prio = OS_PRIO_PCPU,	\
		.aff = PCPU_AFF_PERCPU,	\
		.stk_size = ss,		\
		.flags = fl		\
	}

#define DEFINE_REALTIME(nn, f, a, p, ss, fl) \
	static const struct task_desc __used \
	task_desc_##f __section(.__task_desc) = { \
		.name = nn,		\
		.func = f,		\
		.arg = a,		\
		.prio = p,		\
		.aff = PCPU_AFF_NONE,	\
		.stk_size = ss,		\
		.flags = fl		\
	}

static int inline is_idle_task(struct task *task)
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

static inline int is_realtime_task(struct task *task)
{
	return (task->prio <= OS_LOWEST_PRIO);
}

static inline int is_percpu_task(struct task *task)
{
	return (task->prio == OS_PRIO_PCPU);
}

static inline int is_task_pending(struct task *task)
{
	return ((task->stat & TASK_STAT_PEND_ANY) !=
			TASK_STAT_RDY);
}

static inline int is_task_suspend(struct task *task)
{
	return !!(task->stat & TASK_STAT_SUSPEND);
}

static inline int is_task_ready(struct task *task)
{
	return ((task->stat == TASK_STAT_RDY) ||
			(task->stat == TASK_STAT_RUNNING));
}

int alloc_pid(prio_t prio, int cpuid);
void release_pid(int pid);
int task_ipi_event(struct task *task, struct task_event *ev, int wait);

int create_percpu_task(char *name, task_func_t func, void *arg,
		size_t stk_size, unsigned long flags);

int create_realtime_task(char *name, task_func_t func, void *arg,
		prio_t prio, size_t stk_size, unsigned long flags);

int create_vcpu_task(char *name, task_func_t func, void *arg,
		int aff, size_t stk_size, unsigned long flags);

int create_task(char *name, task_func_t func,
		void *arg, prio_t prio, uint16_t aff,
		uint32_t stk_size, unsigned long opt);

struct task_event *alloc_task_event(void);
void release_task_event(struct task_event *event);

#define task_lock(task)					\
	do {						\
		if (is_realtime_task(task))		\
			kernel_lock();			\
		else					\
			raw_spin_lock(&task->lock);	\
	} while (0)

#define task_unlock(task)				\
	do {						\
		if (is_realtime_task(task)) 		\
			kernel_unlock();		\
		else					\
			raw_spin_unlock(&task->lock);	\
	} while (0)

#define task_lock_irqsave(task, flags)			\
	do {						\
		if (is_realtime_task(task)) 		\
			kernel_lock_irqsave(flags);	\
		else					\
			spin_lock_irqsave(&task->lock, flags);	\
	} while (0)

#define task_unlock_irqrestore(task, flags)		\
	do {						\
		if (is_realtime_task(task)) 		\
			kernel_unlock_irqrestore(flags);\
		else					\
			spin_unlock_irqrestore(&task->lock, flags);	\
	} while (0)

#endif
