#ifndef __MINOS_TASK_INFO_H__
#define __MINOS_TASK_INFO_H__

#include <minos/const.h>

#define TIF_NEED_RESCHED	0
#define TIF_32BIT		1
#define TIF_DONOT_PREEMPT	2
#define TIF_TICK_EXHAUST	3
#define TIF_IN_USER		4
#define TIF_HARDIRQ_MASK	8
#define TIF_SOFTIRQ_MASK	9
#define TIF_NEED_STOP		10
#define TIF_NEED_FREEZE		11

#define __TIF_NEED_RESCHED	(UL(1) << TIF_NEED_RESCHED)
#define __TIF_32BIT		(UL(1) << TIF_32BIT)
#define __TIF_DONOT_PREEMPT	(UL(1) << TIF_DONOT_PREEMPT)
#define __TIF_TICK_EXHAUST	(UL(1) << TIF_TICK_EXHAUST)
#define __TIF_IN_USER		(UL(1) << TIF_IN_USER)
#define __TIF_HARDIRQ_MASK	(UL(1) << TIF_HARDIRQ_MASK)
#define __TIF_SOFTIRQ_MASK	(UL(1) << TIF_SOFTIRQ_MASK)
#define __TIF_NEED_STOP		(UL(1) << TIF_NEED_STOP)
#define __TIF_NEED_FREEZE	(UL(1) << TIF_NEED_FREEZE) // only used for VCPU.

#define __TIF_IN_INTERRUPT	(__TIF_HARDIRQ_MASK | __TIF_SOFTIRQ_MASK)

#ifndef __ASSEMBLY__

#include <config/config.h>
#include <minos/bitops.h>
#include <asm/asm_current.h>

/*
 * this task_info is stored at the top of the task's
 * stack
 */
struct task_info {
	int preempt_count;
	unsigned long flags;
};

static inline struct task *get_current_task(void)
{
	return (struct task *)asm_get_current_task();
}

static inline struct task_info *get_current_task_info(void)
{
	return (struct task_info *)asm_get_current_task_info();
}

static inline void set_current_task(struct task *task)
{
	asm_set_current_task(task);
}

static inline void set_need_resched(void)
{
	set_bit(TIF_NEED_RESCHED, &get_current_task_info()->flags);
	smp_wmb();
}

static inline void clear_need_resched(void)
{
	clear_bit(TIF_NEED_RESCHED, &get_current_task_info()->flags);
	smp_wmb();
}

static inline void clear_do_not_preempt(void)
{
	clear_bit(TIF_DONOT_PREEMPT, &get_current_task_info()->flags);
	smp_wmb();
}

static inline int need_resched(void)
{
	return (get_current_task_info()->flags & __TIF_NEED_RESCHED);
}

static inline void do_not_preempt(void)
{
	set_bit(TIF_DONOT_PREEMPT, &get_current_task_info()->flags);
	wmb();
}

static inline int in_interrupt(void)
{
	return (get_current_task_info()->flags & __TIF_IN_INTERRUPT);
}

#endif

#endif
