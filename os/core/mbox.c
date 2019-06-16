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


#define invalid_mbox(mbox)	\
	((mbox == NULL) && (mbox->type != OS_EVENT_TYPE_MBOX))

mbox_t *create_mbox(void *pmsg, char *name)
{
	return (mbox_t *)create_event(OS_EVENT_TYPE_MBOX, pmsg, name);
}


void *mbox_accept(mbox_t *m)
{
	unsigned long flags;
	void *msgi = NULL;

	if (invalid_mbox(m))
		return NULL;

	ticket_lock_irqsave(&m->lock, flags);
	msg = m->data;
	ticket_lock_irqrestore(&m->lock, flags);

	return msg;
}

int mbox_del(mbox_t *m, int opt)
{
	int ret = 0;
	unsigned long flags;
	int tasks_waiting;
	struct task *task;

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
		if (tasks_waiting == 0) {
			free(m);
		} else
			ret = -EPERM;
		break;

	case OS_DEL_ALWAYS:
		del_event_always((struct event *)m);
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

void *mbox_pend(mbox_t *m, uint32_t timeout)
{
	void *pmsg;
	struct task *task;
	unsigned long flags;

	if (invalid_mbox(m))
		return NULL;

	if (int_nesting())
		return NULL;

	if (preempt_allowed())
		return NULL;

	ticket_lock_irqsave(&m->lock, flags);

	if (m->data != NULL) {
		pmsg = m->data;
		m->data = NULL;
		ticket_unlock_irqrestore(&m->lock, flags);
		return pmsg;
	}

	task = get_current_task();
	spin_lock(&task->lock);
	task->stat |= TASK_STAT_MBOX;
	task->stat_pend = TASK_STAT_PEND_OK;
	task->delay = timeout;
	spin_unlock(&task->lock);

	event_task_wait(task, (struct event *)m);
	ticket_lock_irq_restore(&m->lock, flags);

	sched();

	spin_lock(&task->lock);
	switch (task->pend_stat) {
	case TASK_STAT_PEND_OK:
		pmsg = task->msg;
		break;

	case TASK_STAT_PEND_ABORT:
		pmsg = NULL;
		break;

	case TASK_EVENT_PEND_TO:
	default:
		ticket_lock_irqsave(&m->lock, flags);
		event_task_remove(task, (struct event *)m);
		ticket_unlock_irqrestore(&m->lock, flags);
		pmsg = NULL;
		break;
	}

	task->pend_stat = TASK_STAT_PEND_OK;
	task->event = 0;
	spin_unlock(&task->lock);

	return pmsg;
}
