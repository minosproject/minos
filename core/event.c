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

static atomic_t event_token = { 1 };
static atomic_t event_token_gen = { 0 };

uint32_t new_event_token(void)
{
	uint32_t value;

	while (1) {
		value = (uint32_t)atomic_inc_return_old(&event_token);
		if (value == 0)
			atomic_inc(&event_token_gen);
		else
			break;
	}

	return value;
}

void event_init(struct event *event, int type, void *pdata)
{
	event->type = type;
	spin_lock_init(&event->lock);
	init_list(&event->wait_list);
	event->data = pdata;
}

void __event_task_wait(unsigned long token, int mode, uint32_t to)
{
	struct task *task = current;

	/*
	 * after __event_task_wait, the process will call sched()
	 * by itself, before sched() is called, the task can not
	 * be sched out, since at the same time another thread
	 * may wake up this process, which may case dead lock
	 * with current design.
	 */
	do_not_preempt();

	task->state = TASK_STATE_WAIT_EVENT;
	smp_wmb();

	task->pend_state = TASK_STATE_PEND_OK;
	task->wait_type = mode;
	task->delay = (to == -1 ? 0 : to);
	task->wait_event = token;
}

void event_task_wait(void *ev, int mode, uint32_t to)
{
	struct task *task = get_current_task();
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

	__event_task_wait((unsigned long)ev, mode, to);
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

struct task *event_highest_task_ready(struct event *ev, void *msg,
		uint32_t msk, int pend_state)
{
	struct task *task;

	do {
		task = event_get_waiter(ev);
		if (!task)
			return NULL;
	} while (__wake_up(task, TASK_STATE_PEND_OK, ev->type, msg));

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

void event_pend_down(struct task *task)
{
	task->pend_state = TASK_STATE_PEND_OK;
	task->wait_event = (unsigned long)NULL;
	task->wait_type = 0;
}

long wait_event(void)
{
	struct task *task = current;
	long status;

	sched();

	status = task->pend_state;
	task->pend_state = TASK_STATE_PEND_OK;

	return status;
}

long wait_event_locked(int ev, uint32_t timeout, spinlock_t *lock)
{
	long status;

	__event_task_wait(new_event_token(), ev, timeout);

	if (lock) spin_unlock(lock);
	status = wait_event();
	if (lock) spin_lock(lock);

	return status;
}
