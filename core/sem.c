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
	struct task *task = current;
	unsigned long flags;
	int ret;

	might_sleep();

	spin_lock_irqsave(&sem->lock, flags);
	if (sem->cnt > 0) {
		sem->cnt--;
		spin_unlock_irqrestore(&sem->lock, flags);
		return 0;
	}
	__wait_event(TO_EVENT(sem), OS_EVENT_TYPE_SEM, timeout);
	spin_unlock_irqrestore(&sem->lock, flags);

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
		spin_lock_irqsave(&sem->lock, flags);
		remove_event_waiter(TO_EVENT(sem), task);
		spin_unlock_irqrestore(&sem->lock, flags);
		break;
	}

	event_pend_down();

	return ret;
}

static int sem_post_opt(sem_t *sem, int pend_state, int opt)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&sem->lock, flags);
	ret = wake_up_event_waiter(TO_EVENT(sem), NULL, pend_state, opt);
	if (pend_state != TASK_STATE_PEND_ABORT) {
		if (!ret && (sem->cnt < INT_MAX))
			sem->cnt++;
		else
			ret = -EOVERFLOW;
	}
	spin_unlock_irqrestore(&sem->lock, flags);

	if (ret > 0)
		cond_resched();

	return ret;
}

/*
 * the sem is broken, wake up all the waiter.
 */
int sem_pend_abort(sem_t *sem, int opt)
{
	return sem_post_opt(sem, TASK_STATE_PEND_ABORT,
			OS_EVENT_OPT_BROADCAST);
}

int sem_post(sem_t *sem)
{
	return sem_post_opt(sem, TASK_STATE_PEND_OK,
			OS_EVENT_OPT_NONE);
}
