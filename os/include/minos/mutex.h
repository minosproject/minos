#ifndef __MINOS_MUTEX_H__
#define __MINOS_MUTEX_H__

#include <minos/event.h>

typedef struct event mutex_t;

#define OS_MUTEX_AVAILABLE	0xffff

#define DEFINE_MUTEX(name)	\
	mutex_t name = {	\
		.type = 0xff,	\
	}

mutex_t *mutex_create(char *name);
int mutex_accept(mutex_t *mutex);
int mutex_del(mutex_t *mutex, int opt);
int mutex_pend(mutex_t *m, uint32_t timeout);
int mutex_post(mutex_t *m);

static void inline mutex_init(mutex_t *mutex, char *name)
{
	event_init(to_event(mutex), OS_EVENT_TYPE_MUTEX, NULL, name);
	mutex->cnt = OS_MUTEX_AVAILABLE;
}

#endif
