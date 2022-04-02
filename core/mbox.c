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
#include <minos/sched.h>
#include <minos/mbox.h>
#include <minos/task.h>

void *mbox_accept(mbox_t *m)
{
	unsigned long flags;
	void *msg = NULL;

	spin_lock_irqsave(&m->lock, flags);
	msg = m->data;
	spin_unlock_irqrestore(&m->lock, flags);

	return msg;
}

int mbox_is_pending(mbox_t *m)
{
	return m->data ? 1 : 0;
}

void *mbox_pend(mbox_t *m, uint32_t timeout)
{
	void *pmsg;
	struct task *task;
	unsigned long flags;

	might_sleep();

	spin_lock_irqsave(&m->lock, flags);

	if (m->data != NULL) {
		pmsg = m->data;
		m->data = NULL;
		spin_unlock_irqrestore(&m->lock, flags);
		return pmsg;
	}

	/* no mbox message need to suspend the task */
	task = get_current_task();
	event_task_wait(to_event(m), TASK_EVENT_MBOX, timeout);
	spin_unlock_irqrestore(&m->lock, flags);

	sched();

	switch (task->pend_state) {
	case TASK_STATE_PEND_OK:
		pmsg = task->msg;
		break;

	case TASK_STATE_PEND_ABORT:
		pmsg = NULL;
		break;
	
	case TASK_STATE_PEND_TO:
	default:
		pmsg = NULL;
		spin_lock_irqsave(&m->lock, flags);
		event_task_remove(task, (struct event *)m);
		spin_unlock_irqrestore(&m->lock, flags);
		break;
	}

	task->msg = NULL;
	event_pend_down();

	return pmsg;	
}

int mbox_post(mbox_t *m, void *pmsg)
{
	int ret = 0;
	unsigned long flags;
	struct task *task;

	if (!pmsg)
		return -EINVAL;

	spin_lock_irqsave(&m->lock, flags);
	task = event_highest_task_ready((struct event *)m,
			pmsg, TASK_STATE_PEND_OK);
	if (task) {
		spin_unlock_irqrestore(&m->lock, flags);
		return 0;
	}

	if (m->data != NULL) {
		pr_warn("mbox is full\n");
		ret = -ENOSPC;
	} else {
		m->data = pmsg;
		ret = 0;
	}

	spin_unlock_irqrestore(&m->lock, flags);

	return ret;
}

int mbox_post_opt(mbox_t *m, void *pmsg, int opt)
{
	int ret = 0;
	unsigned long flags;
	int nr_tasks = 0;
	struct task *task;

	if (!pmsg)
		return -EINVAL;

	/* 
	 * check whether the mbox need to broadcast to
	 * all the waitting task
	 */
	spin_lock_irqsave(&m->lock, flags);
	if (opt & OS_POST_OPT_BROADCAST) {
		while (event_has_waiter(to_event(m))) {
			task = event_highest_task_ready((struct event *)m,
					pmsg, TASK_STATE_PEND_OK);
			if (task)
				nr_tasks++;
		}
	} else {
		task = event_highest_task_ready((struct event *)m,
				pmsg, TASK_STATE_PEND_OK);
		if (task)
			nr_tasks++;
	}

	if (nr_tasks) {
		spin_unlock_irqrestore(&m->lock, flags);
		cpus_resched();
		return 0;
	}

	if (m->data != NULL) {
		pr_warn("mbox is full\n");
		ret = -ENOSPC;
	} else {
		m->data = pmsg;
		ret = 0;
	}

	spin_unlock_irqrestore(&m->lock, flags);

	return ret;
}
