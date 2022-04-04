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

void event_task_wait(void *ev, int mode, uint32_t to)
{
	struct task *task = current;
	struct event *event;

	/*
	 * the process of flag is different with other IPC
	 * method
	 */
	if (mode == TASK_EVENT_FLAG) {
		task->flag_node = ev;
	} else {
		event = (struct event *)ev;
		list_add_tail(&event->wait_list, &task->event_list);
	}

	/*
	 * after event_task_wait, the process will call sched()
	 * by itself, before sched() is called, the task can not
	 * be sched out, since at the same time another thread
	 * may wake up this process, which may case dead lock
	 * with current design.
	 */
	do_not_preempt();
	task->state = TASK_STATE_WAIT_EVENT;
	task->pend_state = TASK_STATE_PEND_OK;
	task->wait_type = mode;
	task->delay = (to == -1 ? 0 : to);
}

int event_task_remove(struct task *task, struct event *ev)
{
	/* if task has already timeout or deleted */
	if (task->event_list.next != NULL) {
		list_del(&task->event_list);
		task->event_list.next = NULL;
	}

	return 0;
}

struct task *event_get_waiter(struct event *ev)
{
	struct task *task;

	if (is_list_empty(&ev->wait_list))
		return NULL;

	task = list_first_entry(&ev->wait_list, struct task, event_list);
	event_task_remove(task, ev);

	return task;
}

struct task *event_highest_task_ready(struct event *ev, void *msg, int pend_state)
{
	struct task *task;
	int ret;

	do {
		task = event_get_waiter(ev);
		if (!task)
			return NULL;

		ret = __wake_up(task, TASK_STATE_PEND_OK, ev->type, msg);
		if (ret)
			pr_warn("task state may not correct %d\n", task->name);
	} while (ret);

	return task;
}

void event_del_always(struct event *ev)
{
	struct task *task, *n;

	list_for_each_entry_safe(task, n, &ev->wait_list, event_list) {
		event_task_remove(task, ev);
		wake_up_abort(task);
	}
}

void event_pend_down(void)
{
	struct task *task = current;

	task->pend_state = TASK_STATE_PEND_OK;
	task->wait_event = (unsigned long)NULL;
	task->wait_type = 0;
	task->msg = NULL;
}

long wake(struct event *ev)
{
	struct task *task;
	unsigned long flags;

	spin_lock_irqsave(&ev->lock, flags);
	task = event_highest_task_ready(ev, NULL, TASK_STATE_PEND_OK);
	spin_unlock_irqrestore(&ev->lock, flags);

	return task ? 0 : -ENOENT;
}
