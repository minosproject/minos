#ifndef __MINOS_MUTEX_H__
#define __MINOS_MUTEX_H__

#include <minos/event.h>

typedef struct event mutex_t;

mutex_t *mutex_create(char *name);
int mutex_accept(mutex_t *mutex);
int mutex_del(mutex_t *mutex, int opt);
int mutex_pend(mutex_t *m, uint32_t timeout);
int mutex_post(mutex_t *m);

#endif
