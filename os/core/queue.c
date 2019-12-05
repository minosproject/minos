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

#define invalid_queue(qt) \
	((qt == NULL) || (qt->type != OS_EVENT_TYPE_Q))

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

	event_init(to_event(qt), OS_EVENT_TYPE_Q, (void *)q, name);
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

	if (invalid_queue(qt))
		return NULL;

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

	if (invalid_queue(qt))
		return -EINVAL;

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

	if (invalid_queue(qt))
		return NULL;

	might_sleep();

	spin_lock_irqsave(&qt->lock, flags);
	q = (struct queue *)qt->data;
	if (q->q_cnt > 0) {
		pmsg = queue_pop(q);
		spin_unlock_irqrestore(&qt->lock, flags);
		return pmsg;
	}

	task = get_current_task();
	task_lock(task);
	task->stat |= TASK_STAT_Q;
	task->pend_stat = TASK_STAT_PEND_OK;
	task->delay = timeout;
	task->wait_event = to_event(qt);
	task_unlock(task);

	event_task_wait(task, to_event(qt));
	spin_unlock_irqrestore(&qt->lock, flags);

	sched();

	spin_lock_irqsave(&qt->lock, flags);
	task_lock(task);

	switch (task->pend_stat) {
	case TASK_STAT_PEND_OK:
		pmsg = task->msg;
		break;

	case TASK_STAT_PEND_ABORT:
		pmsg = NULL;
		break;

	case TASK_STAT_PEND_TO:
	default:
		event_task_remove(task, to_event(qt));
		pmsg = NULL;
		break;
	}

	task->pend_stat = TASK_STAT_PEND_OK;
	task->wait_event = NULL;
	task->msg = NULL;

	task_unlock(task);
	spin_unlock_irqrestore(&qt->lock, flags);

	return pmsg;
}

int queue_post_abort(queue_t *qt, int opt)
{
	unsigned long flags;
	int nbr_tasks = 0;

	if (invalid_queue(qt))
		return -EINVAL;

	might_sleep();

	spin_lock_irqsave(&qt->lock, flags);
	if (event_has_waiter(to_event(qt))) {
		switch(opt) {
		case OS_PEND_OPT_BROADCAST:
			while (event_has_waiter(to_event(qt))) {
				event_highest_task_ready(to_event(qt),
						NULL, TASK_STAT_Q,
						TASK_STAT_PEND_ABORT);
				nbr_tasks++;
			}
			break;

		case OS_PEND_OPT_NONE:
		default:
			event_highest_task_ready(to_event(qt), NULL,
					TASK_STAT_Q, TASK_STAT_PEND_ABORT);
			nbr_tasks++;
			break;
		}

		spin_unlock_irqrestore(&qt->lock, flags);
		sched();

		return nbr_tasks;
	}

	spin_unlock_irqrestore(&qt->lock, flags);
	return nbr_tasks;
}

static int __queue_post(queue_t *qt, void *pmsg, int front)
{
	struct queue *q;
	unsigned long flags;

	if (invalid_queue(qt) || !pmsg)
		return -EINVAL;

	spin_lock_irqsave(&qt->lock, flags);
	if (event_has_waiter(to_event(qt))) {
		event_highest_task_ready(to_event(qt), pmsg,
				TASK_STAT_Q, TASK_STAT_PEND_OK);
		spin_unlock_irqrestore(&qt->lock, flags);

		sched();
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

int queue_post_opt(queue_t *qt, int opt, void *pmsg)
{
	unsigned long flags;
	struct queue *q;
	int nr_task = 0;

	if (invalid_queue(qt) || !pmsg)
		return -EINVAL;

	spin_lock_irqsave(&qt->lock, flags);
	if (event_has_waiter(to_event(qt))) {
		if (opt & OS_POST_OPT_BROADCAST) {
			while (event_has_waiter(to_event(qt))) {
				event_highest_task_ready(to_event(qt), pmsg,
					TASK_STAT_Q, TASK_STAT_PEND_OK);
				nr_task++;
			}
		} else {
			event_highest_task_ready(to_event(qt), pmsg,
				TASK_STAT_Q, TASK_STAT_PEND_OK);
			nr_task++;
		}

		if (nr_task) {
			spin_unlock_irqrestore(&qt->lock, flags);
			if ((opt & OS_POST_OPT_NO_SCHED) == 0)
				sched();

			return 0;
		}
	}

	q = (struct queue *)qt->data;
	if (q->q_cnt >= q->q_size) {
		spin_unlock_irqrestore(&qt->lock, flags);
		return -ENOSPC;
	}

	if (opt & OS_POST_OPT_FRONT)
		queue_push_front(q, pmsg);
	else
		queue_push(q, pmsg);

	spin_unlock_irqrestore(&qt->lock, flags);
	return 0;
}
