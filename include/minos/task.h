#ifndef __MINOS_TASK_H__
#define __MINOS_TASK_H__

#include <minos/minos.h>
#include <minos/flag.h>
#include <config/config.h>
#include <minos/task_def.h>

extern struct task *os_task_table[OS_NR_TASKS];

struct task_desc {
	char *name;
	task_func_t func;
	void *arg;
	prio_t prio;
	uint16_t aff;
	size_t size;
	unsigned long flags;
};

struct task_event {
	int id;
	struct task *task;
#define TASK_EVENT_EVENT_READY		0x0
#define TASK_EVENT_FLAG_READY		0x1
	int action;
	void *msg;
	uint32_t msk;
	uint32_t delay;
	flag_t flags;
};

#define task_info(task)	((struct task_info *)task->stack_origin)

#define TASK_INFO_INIT(__ti, task, c) \
	do {		\
		__ti->cpu = c; \
		__ti->task = task; \
		__ti->preempt_count = 0; \
		__ti->flags = 0; \
	} while (0)

#define DEFINE_TASK(nn, f, a, p, af, ss, fl) \
	static const struct task_desc __used \
	task_desc_##f __section(.__task_desc) = { \
		.name = nn,		\
		.func = f,		\
		.arg = a,		\
		.prio = p,		\
		.aff = af,		\
		.size = ss,		\
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
		.size = ss,		\
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
		.size = ss,		\
		.flags = fl		\
	}

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
	return (task->prio <= OS_LOWEST_PRIO);
}

static inline int task_is_percpu(struct task *task)
{
	return (task->prio == OS_PRIO_PCPU);
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

int alloc_pid(prio_t prio, int cpuid);
void release_pid(int pid);

int create_percpu_task(char *name, task_func_t func,
		void *arg, size_t stk_size, unsigned long flags);

int create_realtime_task(char *name, task_func_t func, void *arg,
		prio_t prio, size_t stk_size, unsigned long flags);

int create_vcpu_task(char *name, task_func_t func, void *arg,
		int aff, unsigned long flags);

int create_task(char *name, task_func_t func,
		void *arg, prio_t prio, uint16_t aff,
		size_t stk_size, unsigned long opt);

int release_task(struct task *task);
struct task *pid_to_task(int pid);

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
