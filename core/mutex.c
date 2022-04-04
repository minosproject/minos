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
#include <minos/mutex.h>
#include <minos/sched.h>

#define invalid_mutex(mutex)	\
	((mutex == NULL) && (mutex->type != OS_EVENT_TYPE_MUTEX))

int mutex_accept(mutex_t *mutex)
{
	struct task *task = current;
	int ret = -EBUSY;

	spin_lock(&mutex->lock);
	if (mutex->cnt == OS_MUTEX_AVAILABLE) {
		mutex->owner = task->tid;
		mutex->data = task;
		mutex->cnt = task->prio;
		ret = 0;
	}
	spin_unlock(&mutex->lock);

	return ret;
}

int mutex_pend(mutex_t *m, uint32_t timeout)
{
	struct task *task = current;
	int ret;

	might_sleep();

	/*
	 * mutex_pend and mutex_post can not be used in interrupt
	 * context.
	 */
	spin_lock(&m->lock);
	if (m->cnt == OS_MUTEX_AVAILABLE) {
		m->owner = task->tid;
		m->data = (void *)task;
		m->cnt = task->tid;
		spin_unlock(&m->lock);
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
	event_task_wait(to_event(m), TASK_EVENT_MUTEX, timeout);
	spin_unlock(&m->lock);
	
	sched();

	switch (task->pend_state) {
	case TASK_STATE_PEND_OK:
		ret = 0;
		break;

	case TASK_STATE_PEND_ABORT:
		ret = -EABORT;
		break;

	case TASK_STATE_PEND_TO:
		ret = -ETIMEDOUT;
	default:
		spin_lock(&m->lock);
		event_task_remove(task, (struct event *)m);
		spin_unlock(&m->lock);
		break;
	}

	event_pend_down();
	
	return ret;
}

int mutex_post(mutex_t *m)
{
	struct task *task = current;

	might_sleep();
	ASSERT(m->owner == task->tid);

	spin_lock(&m->lock);

	/* 
	 * find the highest prio task to run, if there is
	 * no task, then set the mutex is available else
	 * resched
	 */
	task = event_highest_task_ready(to_event(m), NULL, TASK_STATE_PEND_OK);
	if (task) {
		m->cnt = task->tid;
		m->data = task;
		m->owner = task->tid;
		spin_unlock(&m->lock);

		return 0;
	}

	m->cnt = OS_MUTEX_AVAILABLE;
	m->data = NULL;
	m->owner = 0;

	spin_unlock(&m->lock);

	return 0;
}
