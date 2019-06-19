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

	ticket_lock_irqsave(&m->lock, flags);
	msg = m->data;
	ticket_unlock_irqrestore(&m->lock, flags);

	return msg;
}

int mbox_del(mbox_t *m, int opt)
{
	int ret = 0;
	unsigned long flags;
	int tasks_waiting;

	if (invalid_mbox(m))
		return -EINVAL;

	if (int_nesting())
		return -EPERM;

	ticket_lock_irqsave(&m->lock, flags);
	if (m->wait_grp || (!is_list_empty(&m->wait_list)))
		tasks_waiting = 1;
	else
		tasks_waiting = 0;

	switch (opt) {
	case OS_DEL_NO_PEND:
		if (tasks_waiting == 0)
			release_event(to_event(m));
		else
			ret = -EPERM;
		break;

	case OS_DEL_ALWAYS:
		event_del_always((struct event *)m);
		release_event(m);
		ticket_unlock_irqrestore(&m->lock, flags);

		if (tasks_waiting)
			sched();

		return 0;
	default:
		ret = -EINVAL;
		break;
	}

	ticket_unlock_irqrestore(&m->lock, flags);
	return ret;
}

void *mbox_pend(mbox_t *m, uint32_t timeout)
{
	void *pmsg;
	struct task *task;
	unsigned long flags;

	if (invalid_mbox(m) || int_nesting() || !preempt_allowed())
		return NULL;

	ticket_lock_irqsave(&m->lock, flags);

	if (m->data != NULL) {
		pmsg = m->data;
		m->data = NULL;
		ticket_unlock_irqrestore(&m->lock, flags);
		return pmsg;
	}

	/* no mbox message need to suspend the task */
	task = get_current_task();
	spin_lock(&task->lock);
	task->stat |= TASK_STAT_MBOX;
	task->pend_stat = TASK_STAT_PEND_OK;
	task->delay = timeout;
	task->wait_event = to_event(m);
	spin_unlock(&task->lock);

	event_task_wait(task, (struct event *)m);
	set_task_suspend(task);
	ticket_unlock_irqrestore(&m->lock, flags);

	sched();

	ticket_lock_irqsave(&m->lock, flags);
	spin_lock(&task->lock);
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
	spin_unlock(&task->lock);
	ticket_unlock_irqrestore(&m->lock, flags);

	return pmsg;
}

int mbox_post(mbox_t *m, void *pmsg)
{
	int ret = 0;
	unsigned long flags;

	if (invalid_mbox(m) || !pmsg)
		return -EINVAL;

	ticket_lock_irqsave(&m->lock, flags);
	if (event_has_waiter(to_event(m))) {
		event_highest_task_ready((struct event *)m, pmsg,
					TASK_STAT_MBOX, TASK_STAT_PEND_OK);
		ticket_unlock_irqrestore(&m->lock, flags);
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

	ticket_unlock_irqrestore(&m->lock, flags);

	return ret;
}

int mbox_post_opt(mbox_t *m, void *pmsg, int opt)
{
	int ret = 0;
	unsigned long flags;

	if (invalid_mbox(m) || !pmsg)
		return -EINVAL;

	/*
	 * check whether the mbox need to broadcast to
	 * all the waitting task
	 */
	ticket_lock_irqsave(&m->lock, flags);
	if (event_has_waiter(to_event(m))) {
		if (opt & OS_POST_OPT_BROADCAST) {
			event_highest_task_ready((struct event *)m, pmsg,
					TASK_STAT_MBOX, TASK_STAT_PEND_OK);
		} else {
			event_highest_task_ready((struct event *)m, pmsg,
					TASK_STAT_MBOX, TASK_STAT_PEND_OK);
		}

		ticket_unlock_irqrestore(&m->lock, flags);
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

	ticket_unlock_irqrestore(&m->lock, flags);

	return ret;
}
