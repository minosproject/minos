/*
 * Copyright (C) 2019 Min Le (lemin9538@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <minos/minos.h>
#include <minos/queue.h>
#include <minos/task.h>
#include <minos/sched.h>
#include <minos/mm.h>

int queue_init(queue_t *qt, int size, char *name)
{
	struct queue *q;

	q = zalloc(sizeof(*q));
	if (!q)
		return -ENOMEM;

	q->q_start = (void **)zalloc(sizeof(void *) * size);
	if (!q->q_start) {
		free(q);
		return -ENOMEM;
	}

	q->q_end = q->q_start + size;
	q->q_in = q->q_start;
	q->q_out = q->q_start;
	q->q_cnt = 0;
	q->q_size = size;

	event_init(TO_EVENT(qt), OS_EVENT_TYPE_QUEUE, (void *)q);

	return 0;
}

static inline void queue_free(queue_t *qt)
{	
	struct queue *q;

	q = qt->data;
	if (q && q->q_start)
		free(q->q_start);
	if (q)
		free(q);
}

static inline void *queue_pop(struct queue *q)
{
	void *pmsg;

	pmsg = *q->q_out++;
	q->q_cnt--;
	if (q->q_out == q->q_end)
		q->q_out = q->q_start;

	return pmsg;
}

static inline void queue_push(struct queue *q, void *pmsg)
{
	*q->q_in++ = pmsg;
	if (q->q_in == q->q_end)
		q->q_in = q->q_start;
	q->q_cnt++;
}

static inline void queue_push_front(struct queue *q, void *pmsg)
{
	if (q->q_out == q->q_start)
		q->q_out = q->q_end;
	q->q_out--;
	*q->q_out = pmsg;
	q->q_cnt++;
}

void *queue_accept(queue_t *qt)
{
	unsigned long flags;
	struct queue *q;
	void *pmsg = NULL;

	spin_lock_irqsave(&qt->lock, flags);

	q = (struct queue *)qt->data;
	if (q->q_cnt > 0)
		pmsg = queue_pop(q);

	spin_unlock_irqrestore(&qt->lock, flags);

	return pmsg;
}

int queue_flush(queue_t *qt)
{
	struct queue *q;
	unsigned long flags;

	spin_lock_irqsave(&qt->lock, flags);
	q = (struct queue *)qt->data;
	q->q_in = q->q_start;
	q->q_out = q->q_start;
	q->q_cnt = 0;
	spin_unlock_irqrestore(&qt->lock, flags);

	return 0;
}

void *queue_pend(queue_t *qt, uint32_t timeout)
{
	void *pmsg;
	struct queue *q;
	unsigned long flags;
	struct task *task;

	might_sleep();

	spin_lock_irqsave(&qt->lock, flags);
	q = (struct queue *)qt->data;
	if (q->q_cnt > 0) {
		pmsg = queue_pop(q);
		spin_unlock_irqrestore(&qt->lock, flags);
		return pmsg;
	}

	task = get_current_task();
	__wait_event(TO_EVENT(qt), OS_EVENT_TYPE_QUEUE, timeout);
	spin_unlock_irqrestore(&qt->lock, flags);

	sched();
	
	switch (task->pend_state) {
	case TASK_STATE_PEND_OK:
		pmsg = task->msg;
		break;

	case TASK_STATE_PEND_ABORT:
		pmsg = NULL;
		break;
	
	case TASK_STATE_PEND_TO:
	default:
		pmsg = NULL;
		spin_lock_irqsave(&qt->lock, flags);
		remove_event_waiter(TO_EVENT(qt), task);
		spin_unlock_irqrestore(&qt->lock, flags);
		break;
	}

	event_pend_down();

	return pmsg;
}

static int __queue_post(queue_t *qt, void *pmsg, int front)
{
	struct queue *q;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&qt->lock, flags);
	ret = wake_up_event_waiter(TO_EVENT(qt), pmsg, TASK_STATE_PEND_OK, 0);
	if (ret) {
		spin_unlock_irqrestore(&qt->lock, flags);
		return 0;
	}

	q = (struct queue *)qt->data;
	if (q->q_cnt >= q->q_size) {
		spin_unlock_irqrestore(&qt->lock, flags);
		return -ENOSPC;
	}

	if (front)
		queue_push_front(q, pmsg);
	else
		queue_push(q, pmsg);

	spin_unlock_irqrestore(&qt->lock, flags);

	return ret;
}

int queue_post_abort(queue_t *qt, int opt)
{
	return 0;
}

int queue_post(queue_t *qt, void *pmsg)
{
	return __queue_post(qt, pmsg, 0);
}

int queue_post_front(queue_t *qt, void *pmsg)
{
	return __queue_post(qt, pmsg, 1);
}
