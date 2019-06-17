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

#include <minos/event.h>

struct event *create_event(int type, void *pdata, char *name)
{
	struct evnet *event;

	if (int_nesting())
		return NULL;

	event = zalloc(sizeof(*event));
	if (!event)
		return NULL;

	event->event_type = type;
	ticketlock_init(&event->lock);
	init_list(&event->wait_list);
	event->data = pdata;
	strncpy(event->name, name, MIN(strlen(name), OS_EVENT_NAME_SIZE));

	return event;
}

void release_event(struct event *event)
{
	free(event);
}

int event_task_ready(struct task *task, void *msg, uint32_t msk, int pend_stat)
{
	spin_lock(&task->lock);
	task->delay = 0;
	task->msg = msg;
	task->stat &= ~msk;		// clear the pending stat
	task->pend_stat = pend_stat;
	task->wait_event = 0;
	spin_unlock(&task->lock);

	if ((task->stat & TASK_STAT_SUSPEND) == TASK_STAT_RDY)
		set_task_ready(task);
}

void event_task_wait(struct task *task, struct event *ev)
{
	uint8_t y;

	task->wait_event = ev;

	if (task->prio <= OS_LOWEST_PRIO) {
		ev->wait_grp |= task->bity;
		ev->wait_tbl[task->by] |= task->bx;
	} else
		list_add_tail(&ev->wait_list, &task->event_list);
}

void event_task_remove(struct task *task, struct event *ev)
{
	if (task->prio > OS_LOWEST_PRIO) {
		list_del(&task->event_list);
		return;
	}
	`
	ev->wait_tbl &= ~task->bitx;
	if (ev->wait_grp[task->by] == 0)
		ev->wait_grp &= ~task->bity;
}

struct task *event_get_waiter(struct event *ev)
{
	uint8_t x, y;
	struct task *task;

	if (ev->wait_grp != 0)
		return get_highest_task(ev->wait_grp, ev->wait_tbl);

	if (!list_is_empty(&ev->wait_list)) {
		return list_first_entry(&ev->wait_list,
				struct task, event_list);
	}

	return NULL;
}

void event_highest_task_ready(struct event *ev, void *msg,
		uint32_t msk, int pend_stat)
{
	struct task *task;

	task = event_get_waiter(ev);
	if (!task)
		return;

	event_task_ready(task, msg, msk, pend_stat);
	event_task_remove(ev, task);
}

void del_event_always(struct event *ev)
{
	struct task *task;

	/* ready all the task waitting for this mutex */
	list_for_each_entry_safe(task, n, &ev->wait_list, event_list) {
		event_task_ready(task, NULL, TASK_STAT_MUTEX,
					TASK_STAT_PEND_OK);
		event_task_remove(task, ev);
	}

	while (mutex->wait_grp != 0) {
		task = get_highest_task(ev->wait_grp, ev->wait_tbl);
		event_task_ready(task, NULL, TASK_STAT_MUTEX,
					TASK_STAT_PEND_OK);
		event_task_remove(task, ev);
	}

	free(ev);
}

static inline int event_has_waiter(struct event *ev)
{
	return ((ev->wait_grp) || (!is_list_empty(&ev->wait_list)));
}
