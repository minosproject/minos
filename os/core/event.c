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

static LIST_HEAD(event_list);
static DEFINE_SPIN_LOCK(event_lock);

void event_init(struct event *event, int type, void *pdata, char *name)
{
	event->type = type;
	ticketlock_init(&event->lock);
	init_list(&event->wait_list);
	event->data = pdata;
	strncpy(event->name, name, MIN(strlen(name), OS_EVENT_NAME_SIZE));

	spin_lock(&event_lock);
	list_add_tail(&event_list, &event->list);
	spin_unlock(&event_lock);
}

struct event *create_event(int type, void *pdata, char *name)
{
	struct event *event;

	if (int_nesting())
		return NULL;

	event = zalloc(sizeof(struct event));
	if (!event)
		return NULL;

	event_init(event, type, pdata, name);

	return event;
}

void release_event(struct event *event)
{
	unsigned long flags;

	spin_lock_irqsave(&event_lock, flags);
	list_del(&event->list);
	spin_unlock_irqrestore(&event_lock, flags);
	free(event);
}

void event_task_wait(struct task *task, struct event *ev)
{
	if (is_realtime_task(task)) {
		ev->wait_grp |= task->bity;
		ev->wait_tbl[task->by] |= task->bitx;
	} else
		list_add_tail(&ev->wait_list, &task->event_list);
}

void event_task_remove(struct task *task, struct event *ev)
{
	if (task->prio > OS_LOWEST_PRIO) {
		list_del(&task->event_list);
		return;
	}

	ev->wait_tbl[task->by] &= ~task->bitx;
	if (ev->wait_tbl[task->by] == 0)
		ev->wait_grp &= ~task->bity;
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

static void event_task_ready(struct task *task, void *msg,
		uint32_t msk, int pend_stat)
{
	struct task_event *tevent;
	int cpuid = smp_processor_id();

	if (is_realtime_task(task) || (task->affinity == cpuid)) {
		task->delay = 0;
		task->msg = msg;
		task->stat &= ~msk;
		task->pend_stat = pend_stat;
		task->wait_event = NULL;
		set_task_ready(task);
	} else {
		/*
		 * send a ipi to let the task's pcpu to update
		 * the task's stat
		 */
		tevent = malloc(sizeof(*tevent));
		if (!tevent)
			panic("no memory for event task event\n");

		tevent->task = task;
		tevent->action = TASK_EVENT_EVENT_READY;
		tevent->msg = msg;
		tevent->msk = msk;
		tevent->pend_stat = pend_stat;

		task_ipi_event(task, tevent, 0);
	}
}

struct task *event_highest_task_ready(struct event *ev, void *msg,
		uint32_t msk, int pend_stat)
{
	struct task *task;
	unsigned long flags = 0;

	task = event_get_waiter(ev);
	if (!task)
		panic("something wrong in event_get_waiter\n");

	/*
	 * need to check whether this task has got
	 * timeout firstly, since even it is timeout
	 * the task will not remove from the waiter
	 * list soon.
	 */
	task_lock_irqsave(task, flags);
	event_task_remove(task, ev);
	event_task_ready(task, msg, msk, pend_stat);
	task_unlock_irqrestore(task, flags);

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
