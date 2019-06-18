#ifndef __MINOS_MBOX_H__
#define __MINOS_MBOX_H__

#include <minos/event.h>

typedef struct event mbox_t;

mbox_t *mbox_create(void *pmsg, char *name);
void *mbox_accept(mbox_t *m);
int mbox_del(mbox_t *m, int opt);
void *mbox_pend(mbox_t *m, uint32_t timeout);
int mbox_post(mbox_t *m, void *pmsg);
int mbox_post_opt(mbox_t *m, void *pmsg, int opt);

#endif
