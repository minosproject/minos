#ifndef __MINOS_QUEUE_H__
#define __MINOS_QUEUE_H__

#include <minos/event.h>

typedef struct event queue_t;

struct queue {
	void **q_start;		/* contain the pointer of the data */
	void **q_end;		/* end of the queue buffer */
	void **q_in;		/* next message in */
	void **q_out;		/* next message out */
	int q_size;		/* the total size of the queue */
	int q_cnt;		/* current queue entriy size */
};

int queue_init(queue_t *qt, int size, char *name);
void *queue_accept(queue_t *qt);
int queue_flush(queue_t *qt);
void *queue_pend(queue_t *qt, uint32_t timeout);
int queue_post_abort(queue_t *qt, int opt);
int queue_post(queue_t *qt, void *pmsg);
int queue_post_front(queue_t *qt, void *pmsg);
int queue_post_opt(queue_t *qt, int opt, void *pmsg);

#endif
