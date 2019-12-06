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
#include <minos/task.h>
#include <minos/sched.h>
#include <minos/event.h>
#include <minos/mm.h>
#include <minos/smp.h>

void event_init(struct event *event, int type, void *pdata)
{
	event->type = type;
	spin_lock_init(&event->lock);
	init_list(&event->wait_list);
	event->data = pdata;
}

void event_task_wait(struct task *task, void *ev, int stat, uint32_t to)
{
	unsigned long flags;
	struct event *event;

	task_lock_irqsave(task, flags);

	task->stat |= stat;
	task->pend_stat = TASK_STAT_PEND_OK;
	task->delay = to;

	if (stat == TASK_STAT_FLAG) {
		task->flag_node = ev;
	} else {
		event = (struct event *)ev;
		task->wait_event = event;
		if (task_is_realtime(task)) {
			event->wait_grp |= task->bity;
			event->wait_tbl[task->by] |= task->bitx;
		} else
			list_add_tail(&event->wait_list, &task->event_list);
	}

	set_task_sleep(task, to);
	task_unlock_irqrestore(task, flags);
}

int event_task_remove(struct task *task, struct event *ev)
{
	int pending = task_is_pending(task);

	/* if task has already timeout or deleted */
	if (!task_is_realtime(task)) {
		if (task->event_list.next != NULL) {
			list_del(&task->event_list);
			task->event_list.next = NULL;

			/* if the task is pending now then success */
			if (pending)
				return 0;
		}

		return -EPERM;
	} else {
		ev->wait_tbl[task->by] &= ~task->bitx;
		if (ev->wait_tbl[task->by] == 0)
			ev->wait_grp &= ~task->bity;

		if (pending)
			return 0;
	}

	return -EPERM;
}

struct task *event_get_waiter(struct event *ev)
{
	if (ev->wait_grp != 0)
		return get_highest_task(ev->wait_grp, ev->wait_tbl);

	if (!is_list_empty(&ev->wait_list)) {
		return list_first_entry(&ev->wait_list,
				struct task, event_list);
	}

	return NULL;
}

static void inline event_task_ready(struct task *task, void *msg,
		uint32_t msk, int pend_stat)
{
	task->pend_stat = pend_stat;

	task->msg = msg;
	task->stat &= ~msk;
	task->wait_event = NULL;

	set_task_ready(task, 0);
}

struct task *event_highest_task_ready(struct event *ev, void *msg,
		uint32_t msk, int pend_stat)
{
	int retry = 0;
	struct task *task;
	unsigned long flags = 0;

again:
	task = event_get_waiter(ev);
	if (!task)
		return NULL;

	/*
	 * need to check whether this task has got
	 * timeout firstly, since even it is timeout
	 * the task will not remove from the waiter
	 * list soon.
	 *
	 * this function will race with task_timeout_handler
	 * so checkt the pend_stat to avoid race
	 */
	task_lock_irqsave(task, flags);
	if (!event_task_remove(task, ev)) {
		event_task_ready(task, msg, msk, pend_stat);
		retry = 0;
	} else {
		retry = 1;
	}
	task_unlock_irqrestore(task, flags);

	if (retry)
		goto again;

	return task;
}

void event_del_always(struct event *ev)
{
	struct task *task, *n;

	/*
	 * ready all the task waitting for this mutex
	 * set the pend stat to PEND_ABORT to indicate
	 * that the event is not valid when the task
	 * has been waked up
	 */
	list_for_each_entry_safe(task, n, &ev->wait_list, event_list) {
		event_task_remove(task, ev);
		event_task_ready(task, NULL, TASK_STAT_MUTEX,
					TASK_STAT_PEND_ABORT);
	}

	while (ev->wait_grp != 0) {
		task = get_highest_task(ev->wait_grp, ev->wait_tbl);
		event_task_ready(task, NULL, TASK_STAT_MUTEX,
					TASK_STAT_PEND_ABORT);
		event_task_remove(task, ev);
	}
}
