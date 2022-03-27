#ifndef __MINOS_EVENT_H__
#define __MINOS_EVENT_H__

#include <minos/preempt.h>
#include <minos/types.h>

#define OS_EVENT_TYPE_UNUSED	0
#define OS_EVENT_TYPE_MBOX	1
#define OS_EVENT_TYPE_Q		2
#define OS_EVENT_TYPE_SEM	3
#define OS_EVENT_TYPE_MUTEX	4
#define OS_EVENT_TYPE_FLAG	5

#define OS_PEND_OPT_NONE        0
#define OS_PEND_OPT_BROADCAST   1

#define OS_POST_OPT_NONE        0x00
#define OS_POST_OPT_BROADCAST   0x01
#define OS_POST_OPT_FRONT       0x02
#define OS_POST_OPT_NO_SCHED    0x04

struct task;

struct event {
	int type;				/* event type */
	tid_t owner;				/* event owner the pid */
	uint32_t cnt;				/* event cnt */
	void *data;				/* event pdata for transfer */
	spinlock_t lock;			/* the lock of the event for smp */
	struct list_head wait_list;		/* non realtime task waitting list */
	struct list_head list;			/* list to the task's owner list */
};

#define to_event(e)	(struct event *)e

int event_task_remove(struct task *task, struct event *ev);
struct task *event_get_waiter(struct event *ev);

struct task *event_highest_task_ready(struct event *ev, void *msg,
		uint32_t msk, int pend_stat);

void event_init(struct event *event, int type, void *pdata);
void event_task_wait(void *ev, int stat, uint32_t to);
void __event_task_wait(unsigned long token, int mode, uint32_t to);
void event_pend_down(struct task *task);

uint32_t new_event_token(void);
long wait_event(void);
long wait_event_locked(int ev, uint32_t timeout, spinlock_t *lock);

static inline int event_has_waiter(struct event *ev)
{
	return (!is_list_empty(&ev->wait_list));
}

#endif
