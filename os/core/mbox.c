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

#define invalid_mbox(mbox)	\
	((mbox == NULL) && (mbox->type != OS_EVENT_TYPE_MBOX))

mbox_t *mbox_create(void *pmsg, char *name)
{
	return (mbox_t *)create_event(OS_EVENT_TYPE_MBOX, pmsg, name);
}

void *mbox_accept(mbox_t *m)
{
	unsigned long flags;
	void *msg = NULL;

	if (invalid_mbox(m))
		return NULL;

	spin_lock_irqsave(&m->lock, flags);
	msg = m->data;
	spin_unlock_irqrestore(&m->lock, flags);

	return msg;
}

int mbox_del(mbox_t *m, int opt)
{
	int ret = 0;
	unsigned long flags;
	int tasks_waiting;

	if (invalid_mbox(m))
		return -EINVAL;

	spin_lock_irqsave(&m->lock, flags);
	if (m->wait_grp || (!is_list_empty(&m->wait_list)))
		tasks_waiting = 1;
	else
		tasks_waiting = 0;

	switch (opt) {
	case OS_DEL_NO_PEND:
		if (tasks_waiting == 0) {
			spin_unlock_irqrestore(&m->lock, flags);
			release_event(to_event(m));
			return 0;
		} else
			ret = -EPERM;
		break;

	case OS_DEL_ALWAYS:
		event_del_always((struct event *)m);
		spin_unlock_irqrestore(&m->lock, flags);
		release_event(m);

		if (tasks_waiting)
			sched();

		return 0;
	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock_irqrestore(&m->lock, flags);
	return ret;
}

void *mbox_pend(mbox_t *m, uint32_t timeout)
{
	void *pmsg;
	struct task *task;
	unsigned long flags;

	if (invalid_mbox(m))
		return NULL;

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
	task_lock(task);
	task->stat |= TASK_STAT_MBOX;
	task->pend_stat = TASK_STAT_PEND_OK;
	task->delay = timeout;
	task->wait_event = to_event(m);
	task_unlock(task);

	event_task_wait(task, (struct event *)m);
	spin_unlock_irqrestore(&m->lock, flags);

	sched();

	spin_lock_irqsave(&m->lock, flags);
	task_lock(task);
	switch (task->pend_stat) {
	case TASK_STAT_PEND_OK:
		pmsg = task->msg;
		break;

	case TASK_STAT_PEND_ABORT:
		pmsg = NULL;
		break;

	case TASK_STAT_PEND_TO:
	default:
		event_task_remove(task, (struct event *)m);
		pmsg = NULL;
		break;
	}

	task->pend_stat = TASK_STAT_PEND_OK;
	task->wait_event = NULL;
	task_unlock(task);
	spin_unlock_irqrestore(&m->lock, flags);

	return pmsg;
}

int mbox_post(mbox_t *m, void *pmsg)
{
	int ret = 0;
	unsigned long flags;
	struct task *task;

	if (invalid_mbox(m) || !pmsg)
		return -EINVAL;

	spin_lock_irqsave(&m->lock, flags);
	if (event_has_waiter(to_event(m))) {
		task = event_highest_task_ready((struct event *)m, pmsg,
				TASK_STAT_MBOX, TASK_STAT_PEND_OK);
		if (task) {
			spin_unlock_irqrestore(&m->lock, flags);
			sched();
		}

		return 0;
	}

	if (m->data != NULL) {
		pr_debug("mbox-%s is full\n", m->name);
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

	if (invalid_mbox(m) || !pmsg)
		return -EINVAL;

	/*
	 * check whether the mbox need to broadcast to
	 * all the waitting task
	 */
	spin_lock_irqsave(&m->lock, flags);
	if (event_has_waiter(to_event(m))) {
		if (opt & OS_POST_OPT_BROADCAST) {
			while (event_has_waiter(to_event(m))) {
				event_highest_task_ready((struct event *)m,
						pmsg, TASK_STAT_MBOX,
						TASK_STAT_PEND_OK);
				nr_tasks++;
			}
		} else {
			event_highest_task_ready((struct event *)m,
					pmsg, TASK_STAT_MBOX,
					TASK_STAT_PEND_OK);
			nr_tasks++;
		}

		spin_unlock_irqrestore(&m->lock, flags);
		sched();

		return 0;
	}

	if (m->data != NULL) {
		pr_debug("mbox-%s is full\n", m->name);
		ret = -ENOSPC;
	} else {
		m->data = pmsg;
		ret = 0;
	}

	spin_unlock_irqrestore(&m->lock, flags);

	return ret;
}
