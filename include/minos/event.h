#ifndef __MINOS_EVENT_H__
#define __MINOS_EVENT_H__

#include <minos/preempt.h>
#include <minos/types.h>
#include <minos/spinlock.h>

#define OS_EVENT_OPT_NONE 0x0
#define OS_EVENT_OPT_BROADCAST 0x1

enum {
	OS_EVENT_TYPE_UNKNOWN = 0,
	OS_EVENT_TYPE_MBOX,
	OS_EVENT_TYPE_QUEUE,
	OS_EVENT_TYPE_SEM,
	OS_EVENT_TYPE_MUTEX,
	OS_EVENT_TYPE_FLAG,
	OS_EVENT_TYPE_NORMAL,

	OS_EVENT_TYPE_TIMER,

	OS_EVENT_TYPE_MAX
};

struct task;

struct event {
	int type;				/* event type */
	tid_t owner;				/* event owner the tid */
	uint32_t cnt;				/* event cnt */
	void *data;				/* event pdata for transfer */
	spinlock_t lock;			/* the lock of the event for smp */
	struct list_head wait_list;		/* non realtime task waitting list */
};

#define TO_EVENT(e)	(struct event *)(e)

void event_init(struct event *event, int type, void *pdata);
int remove_event_waiter(struct event *ev, struct task *task);
void event_pend_down(void);

void __wait_event(void *ev, int event, uint32_t to);
long wake(struct event *ev);

int __wake_up_event_waiter(struct event *ev, void *msg,
		int pend_state, int opt);

#define wake_up_event_waiter(ev, msg, pend_state, opt) \
	__wake_up_event_waiter(TO_EVENT(ev), msg, pend_state, opt)

#define wait_event(ev, condition, _to)			\
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
	__wait_event(ev, OS_EVENT_TYPE_NORMAL, _to); 	\
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
