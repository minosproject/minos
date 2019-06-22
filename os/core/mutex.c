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
#include <minos/ticketlock.h>
#include <minos/mutex.h>
#include <minos/sched.h>

#define OS_MUTEX_AVAILABLE	0xffff

#define invalid_mutex(mutex)	\
	((mutex == NULL) && (mutex->type != OS_EVENT_TYPE_MUTEX))

mutex_t *mutex_create(char *name)
{
	return (mutex_t *)create_event(OS_EVENT_TYPE_MUTEX, NULL, name);
}

int mutex_accept(mutex_t *mutex)
{
	int ret = -EBUSY;
	unsigned long flags;
	struct task *task = get_current_task();

	if (invalid_mutex(mutex))
		return -EPERM;

	if(int_nesting())
		return -EPERM;

	/* if the mutex is avaliable now, lock it */
	ticket_lock_irqsave(&mutex->lock, flags);
	if (mutex->cnt == OS_MUTEX_AVAILABLE) {
		mutex->owner = task->pid;
		mutex->data = task;
		mutex->cnt = task->prio;
		ret = 0;
	}
	ticket_unlock_irqrestore(&mutex->lock, flags);

	return ret;
}

int mutex_del(mutex_t *mutex, int opt)
{
	unsigned long flags;
	int tasks_waiting;
	int ret;
	struct task *task;

	if (invalid_mutex(mutex))
		return -EINVAL;

	ticket_lock_irqsave(&mutex->lock, flags);

	if (event_has_waiter(to_event(mutex)))
		tasks_waiting = 1;
	else
		tasks_waiting = 0;

	switch (opt) {
	case OS_DEL_NO_PEND:
		if (tasks_waiting) {
			pr_err("can not delete mutex task waitting for it\n");
			ret = -EPERM;
		} else {
			ticket_unlock_irqrestore(&mutex->lock, flags);
			release_event(to_event(mutex));
			return 0;
		}
		break;

	case OS_DEL_ALWAYS:
		/*
		 * need to unlock the cpu if the cpu is lock
		 * by this task, the mutex can not be accessed
		 * by the task after it has been locked.
		 */
		task = (struct task *)mutex->data;
		if (task != NULL) {
			atomic_set(&task->lock_cpu, 1);
			task->lock_event = NULL;
		}

		event_del_always(to_event(mutex));
		ticket_unlock_irqrestore(&mutex->lock, flags);
		release_event(to_event(mutex));

		if (tasks_waiting)
			sched();

		return 0;
	default:
		ret = -EINVAL;
		break;
	}

	ticket_unlock_irqrestore(&mutex->lock, flags);
	return ret;
}

int mutex_pend(mutex_t *m, uint32_t timeout)
{
	int ret;
	unsigned long flags;
	struct task *owner;
	struct task *task = get_current_task();

	if (invalid_mutex(m) || int_nesting() || !preempt_allowed())
		return -EINVAL;

	ticket_lock_irqsave(&m->lock, flags);
	if (m->cnt == OS_MUTEX_AVAILABLE) {
		m->owner = task->pid;
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
	owner = (struct task *)m->data;
	if (owner->prio > task->prio) {
		atomic_set(&task->lock_cpu, 1);
		task->lock_event = to_event(m);
	}

	/* set the task's state and suspend the task */
	task_lock(task);
	task->stat |= TASK_STAT_MUTEX;
	task->pend_stat = TASK_STAT_PEND_OK;
	task->delay = timeout;
	task->wait_event = to_event(m);
	task_unlock(task);

	event_task_wait(task, to_event(m));
	ticket_unlock_irqrestore(&m->lock, flags);

	sched();

	ticket_lock_irqsave(&m->lock, flags);
	task_lock(task);

	switch (task->pend_stat) {
	case TASK_STAT_PEND_OK:
		ret = 0;
		break;

	case TASK_STAT_PEND_ABORT:
		ret = EABORT;
		break;

	case TASK_STAT_PEND_TO:
	default:
		ret = -ETIMEDOUT;
		event_task_remove(task, (struct event *)m);
		break;
	}

	task->pend_stat = TASK_STAT_PEND_OK;
	task->wait_event = 0;
	task_unlock(task);
	ticket_unlock_irqrestore(&m->lock, flags);

	return ret;
}

int mutex_post(mutex_t *m)
{
	unsigned long flags;
	struct task *task = get_current_task();

	if (invalid_mutex(m) || int_nesting() || !preempt_allowed())
		return -EPERM;

	ticket_lock_irqsave(&m->lock, flags);
	if (task != (struct task *)m->data) {
		ticket_unlock_irqrestore(&m->lock, flags);
		return -EINVAL;
	}

	/*
	 * find the highest prio task to run, if there is
	 * no task, then set the mutex is available else
	 * resched
	 */
	if (event_has_waiter(to_event(m))) {
		task = event_highest_task_ready((struct event *)m, NULL,
				TASK_STAT_MUTEX, TASK_STAT_PEND_OK);
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
