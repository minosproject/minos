#ifndef __MINOS_EVENT_H__
#define __MINOS_EVENT_H__

#include <minos/ticketlock.h>
#include <minos/task.h>

#define OS_EVENT_TBL_SIZE	(OS_REALTIME_TASK / 8)
#define OS_EVENT_NAME_SIZE	31

#define OS_EVENT_TYPE_UNUSED	0
#define OS_EVENT_TYPE_MBOX	1
#define OS_EVENT_TYPE_Q		2
#define OS_EVENT_TYPE_SEM	3
#define OS_EVENT_TYPE_MUTEX	4
#define OS_EVENT_TYPE_FLAG	5

struct event {
	uint8_t type;		/* event type */
	prio_t high_prio	/* prio used to up to */
	uint16_t owner;		/* event owner the pid */
	uint16_t cnt;		/* event cnt */
	void *data;		/* event pdata for transfer */
	ticketlock_t lock;	/* the lock of the event for smp */
	prio_t wait_grp;		/* realtime task waiting on this event */
	prio_t wait_tbl[OS_EVENT_TABLE_SIZE]; /* wait bitmap */
	struct list_head wait_list;	/* non realtime task waitting list */
	struct list_head list;
	char event_name[OS_EVENT_NAME_SIZE];
};

typedef struct event mbox_t;
typedef struct event queue_t;
typedef struct event sem_t;
typedef struct event mutex_t;
typedef struct event flag_t;

#endif
