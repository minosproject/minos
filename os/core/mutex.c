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
#include <minos/event.h>
#include <minos/task.h>

#define OS_MUTEX_AVAILABLE	0xffff

#define invalid_mutex()	((mutex == NULL) && (mutex->type != OS_EVENT_TYPE_MUTEX))

mutex_t create_mutex(char *name)
{
	return (mutex_t *)create_event(OS_EVENT_TYPE_MUTEX, name);
}

int mutex_accept(mutex_t *mutex)
{
	int ret = 0;
	unsigned long flags;
	struct task *task = get_current_task();

	if (invalid_mutex())
		return -EPERM;

	if(int_nesting())
		return -EPERM;

	ticket_lock_irqsave(&mutex->lock, flags);
	if (mutex->cnt == OS_MUTEX_AVAILABLE) {
		mutex->owner = task->pid;
		mutex->data = task;
		mutex->cnt = task->prio;
	}
	ticket_lock_irqrestore(&mutex->lock, flags);

	return ret;
}

int mutex_del(mutex_t *mutex, int opt)
{
	unsigned long flags;
	int tasks_waiting;
	int ret;
	prio_t prio;
	struct task *task, *n;

	if (invalid_mutex())
		return -EINVAL;

	ticket_lock_irqsave(&mutex->lock, flags);

	if (mutex->wait_grp || !is_list_empty(&mutex->wait_list))
		task_waiting = 1;
	else
		task_waiting = 0;

	switch (opt) {
	case OS_DEL_NO_PEND:
		if (task_waiting) {
			pr_error("can not delete mutex task waitting for it\n");
			ret = -EPERM;
		} else {
			release_pid(mutex->high_prio);
			free(mutex);
			ret = 0;
		}
		break;

	case OS_DEL_ALWAYS:
		/*
		 * need to unlock the cpu if the cpu is lock
		 * by this task
		 */
		task = (struct task *)mutex->data;
		if (task != NULL) {
			atomic_set(&task->lock_cpu);
			task->lock_event = NULL;
		}

		/* ready all the task waitting for this mutex */
		list_for_each_entry_safe(task, n, &mutex->wait_list, event_list) {
			event_task_ready(task, NULL, TASK_STAT_MUTEX,
					TASK_STAT_PEND_OK);
			event_task_remove(task, (struct evnet *)mutex);
		}

		while (mutex->wait_grp != 0) {
			task = get_highest_task(mutex->wait_grp,
					mutex->wait_tbl);
			event_task_ready(task, NULL, TASK_STAT_MUTEX,
					TASK_STAT_PEND_OK);
			event_task_remove(task, (struct evnet *)mutex);
		}

		ticket_unlock_irqrestore(&mutex->lock, flags);
		if (task_waiting)
			sched();

		return 0;

	default:
		ret = -EINVAL;
		break;
	}

	ticket_unlock_irqrestore(&mutex->lock, flags);
	return ret;
}

int mutex_lock(mutex_t *m, uint32_t timeout)
{
	unsigned long flags;
	struct task *task = get_current_task();

	if (invalid_mutex() || int_nesting() || !preempt_allowed())
		return -EINVAL;

	ticket_lock_irqsave(&m->lock, flags);
	if (m->cnt == OS_MUTEX_AVAILABLE) {
		m->pid = task->pid;
		m->data = (void *)task;
		ticket_unlock_irqrestore(&m->lock, flags);
		return 0;
	}


	/*
	 * priority inversion - only check the realtime task
	 *
	 * check the current task's prio to see whether the
	 * current task's prio is lower than mutex's owner, if
	 * yes, need to increase the owner's prio
	 *
	 * on smp it not easy to deal with the priority, then
	 * just lock the cpu until the task(own the mutex) has
	 * finish it work, but there is a big problem, if the
	 * task need to get two mutex, how to deal with this ?
	 */
	if (m->prio > task->prio) {
		atomic_set(&task->lock_cpu, 1);
		task->lock_event = (struct evnet *)m;
	}

	/* set the task's state and suspend the task */
	spin_lock(&task->lock);
	task->stat |= TASK_STAT_MUTEX;
	task->pend_stat = TASK_STAT_PEND_OK;
	task->delay = timeout;
	spin_unlock(&task->lock);

	event_task_wait(task, (struct event *)m);
	set_task_suspend(task);

	ticket_unlock_irqrestore(&m->lock, flags);

	sched();

	switch (task->pend_stat) {
	case TASK_STAT_PEND_OK:
		ret = 0;
		break;

	case TASK_STAT_PEND_ABORT:
		ret = EABORT;
		break;

	case TASK_EVENT_PEND_TO:
	default:
		ret = -ETIMEDOUT;
		break;
	}

	/* remove this task to the event wait list */
	ticket_lock_irqsave(&m->lock, flags);
	event_task_remove(task, (struct event *)m);
	ticket_unlock_irqrestore(&m->lock, flags);

	spin_lock(&task->lock);
	task->pend_stat = TASK_STAT_PEND_OK;
	task->event = 0;
	spin_unlock(&task->lock);

	return ret;
}

int mutex_unlock(mutex_t *m)
{
	unsigned long flags;
	struct task *task = get_current_task();

	if (invalid_mutex() || int_nesting() || !preempt_allowed())
		return -EPERM;

	ticket_lock_irqsave(&m->lock, flags);
	if (task != (struct task *)m->data) {
		ticket_unlock_irqrestore(&m->lock, flags);
		return -EINVAL;
	}

	/* find the highest prio task to run */
	if ((m->wait_grp != 0) || !(is_list_empty(&m->wait_list))) {
		task = event_get_ready((struct event *)m);
		m->cnt = task->pid;
		m->data = task;
		ticket_unlock_irqrestore(&m->lock, flags);

		sched();

		return 0;
	}

	m->cnt = OS_MUTEX_AVAILABLE;
	m->data = NULL;
	ticket_unlock_irqrestore(&m->lock, flags);

	return 0;
}
