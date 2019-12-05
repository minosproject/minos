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
#include <minos/sem.h>
#include <minos/sched.h>

uint32_t sem_accept(sem_t *sem)
{
	uint32_t cnt;
	unsigned long flags;

	spin_lock_irqsave(&sem->lock, flags);
	cnt = sem->cnt;
	if (cnt > 0)
		sem->cnt--;
	spin_unlock_irqrestore(&sem->lock, flags);

	return cnt;
}

int sem_pend(sem_t *sem, uint32_t timeout)
{
	int ret;
	unsigned long flags;
	struct task *task;

	might_sleep();

	spin_lock_irqsave(&sem->lock, flags);
	if (sem->cnt > 0) {
		sem->cnt--;
		spin_unlock_irqrestore(&sem->lock, flags);
		return 0;
	}

	task = get_current_task();
	event_task_wait(task, to_event(sem), TASK_STAT_SEM, timeout);
	spin_unlock_irqrestore(&sem->lock, flags);

	sched();

	switch (task->pend_stat) {
	case TASK_STAT_PEND_OK:
		ret = 0;
		break;
	case TASK_STAT_PEND_ABORT:
		ret = -EABORT;
		break;
	case TASK_STAT_PEND_TO:
	default:
		ret = -ETIMEDOUT;
		spin_lock_irqsave(&sem->lock, flags);
		event_task_remove(task, to_event(sem));
		spin_unlock_irqrestore(&sem->lock, flags);
		break;
	}

	task->pend_stat = TASK_STAT_PEND_OK;
	task->wait_event = NULL;

	return ret;
}

int sem_pend_abort(sem_t *sem, int opt)
{
	int nbr_tasks = 0;
	unsigned long flags;
	struct task *task;

	might_sleep();

	spin_lock_irqsave(&sem->lock, flags);
	if (event_has_waiter((struct event *)sem)) {
		switch (opt) {
		case OS_PEND_OPT_BROADCAST:
			while (event_has_waiter((struct event *)sem)) {
				task = event_highest_task_ready((struct event *)sem,
					NULL, TASK_STAT_SEM, TASK_STAT_PEND_ABORT);
				if (task)
					nbr_tasks++;
			}
			break;
		case OS_PEND_OPT_NONE:
		default:
			task = event_highest_task_ready((struct event *)sem,
				NULL, TASK_STAT_SEM, TASK_STAT_PEND_OK);
			if (task)
				nbr_tasks++;
			break;
		}

		spin_unlock_irqrestore(&sem->lock, flags);

		if (nbr_tasks) {
			cpus_resched();
			return nbr_tasks;
		}
	}

	spin_unlock_irqrestore(&sem->lock, flags);
	return 0;
}

int sem_post(sem_t *sem)
{
	unsigned long flags;
	struct task *task;

	spin_lock_irqsave(&sem->lock, flags);
	task = event_highest_task_ready((struct event *)sem,
			NULL, TASK_STAT_SEM, TASK_STAT_PEND_OK);
	if (task) {
		spin_unlock_irqrestore(&sem->lock, flags);
		sched_task(task);
		return 0;
	}

	if (sem->cnt < 65535)
		sem->cnt++;

	spin_unlock_irqrestore(&sem->lock, flags);

	return 0;
}
