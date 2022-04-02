#ifndef __MINOS_EVENT_H__
#define __MINOS_EVENT_H__

#include <minos/preempt.h>
#include <minos/types.h>
#include <minos/spinlock.h>

#define OS_EVENT_TYPE_UNKNOWN	0
#define OS_EVENT_TYPE_MBOX	1
#define OS_EVENT_TYPE_Q		2
#define OS_EVENT_TYPE_SEM	3
#define OS_EVENT_TYPE_MUTEX	4
#define OS_EVENT_TYPE_FLAG	5
#define OS_EVENT_TYPE_NORMAL	6

#define OS_PEND_OPT_NONE        0
#define OS_PEND_OPT_BROADCAST   1

#define OS_POST_OPT_NONE        0x00
#define OS_POST_OPT_BROADCAST   0x01
#define OS_POST_OPT_FRONT       0x02
#define OS_POST_OPT_NO_SCHED    0x04

struct task;

struct event {
	int type;				/* event type */
	tid_t owner;				/* event owner the tid */
	uint32_t cnt;				/* event cnt */
	void *data;				/* event pdata for transfer */
	spinlock_t lock;			/* the lock of the event for smp */
	struct list_head wait_list;		/* non realtime task waitting list */
};

#define to_event(e)	(struct event *)e

struct task *event_get_waiter(struct event *ev);
struct task *event_highest_task_ready(struct event *ev,
		void *msg, int pend_stat);

void event_init(struct event *event, int type, void *pdata);
void event_task_wait(void *ev, int mode, uint32_t to);
void event_pend_down(void);
int event_task_remove(struct task *task, struct event *ev);

static inline int event_has_waiter(struct event *ev)
{
	return (!is_list_empty(&ev->wait_list));
}

long wait_timeout(struct event *ev, uint32_t timeout);
long wake(struct event *ev);

static long inline wait(struct event *ev)
{
	return wait_timeout(ev, 0);
}

#define wait_on(ev, condition, _timeout)		\
({							\
	__label__ __out;				\
	__label__ __out1;				\
	unsigned long flags;				\
	long ret = -EBUSY;				\
	int need_wait = 1;				\
							\
	if (!condition)					\
		goto __out1;				\
							\
	spin_lock_irqsave(&(ev)->lock, flags);		\
	if (!condition) {				\
		need_wait = 0;				\
		goto __out;				\
	}						\
	event_task_wait(ev, TASK_EVENT_ANY, _timeout);	\
__out:							\
	spin_unlock_irqrestore(&(ev)->lock, flags);	\
							\
	if (need_wait) {				\
		sched();				\
		ret = current->pend_state;		\
		event_pend_down();			\
	}						\
							\
__out1:	ret;						\
})

#endif
