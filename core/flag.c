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
#include <minos/flag.h>
#include <minos/event.h>
#include <minos/mm.h>
#include <minos/task.h>
#include <minos/sched.h>
#include <minos/spinlock.h>

struct flag_node {
	struct list_head list;
	struct task *task;
	void *flag_grp;
	flag_t flags;
	int wait_type;
};

static inline flag_t flag_wait_set_all(struct flag_grp *grp,
		flag_t flags, int consume)
{
	flag_t flags_rdy;

	flags_rdy = grp->flags & flags;
	if (flags_rdy == flags) {
		if (consume)
			grp->flags &= ~flags_rdy;
	} else
		flags_rdy = 0;

	return flags_rdy;
}

static inline flag_t flag_wait_set_any(struct flag_grp *grp,
		flag_t flags, int consume)
{
	flag_t flags_rdy;

	flags_rdy = grp->flags & flags;
	if ((flags_rdy != 0) && consume)
		grp->flags &= ~flags_rdy;
	
	return flags_rdy;
}

static inline flag_t flag_wait_clr_all(struct flag_grp *grp,
		flag_t flags, int consume)
{
	flag_t flags_rdy;

	flags_rdy = ~grp->flags & flags;
	if (flags_rdy == flags) {
		if (consume)
			grp->flags |= flags_rdy;
	} else
		flags_rdy = 0;

	return flags_rdy;
}

static inline flag_t flag_wait_clr_any(struct flag_grp *grp,
		flag_t flags, int consume)
{
	flag_t flags_rdy;

	flags_rdy = ~grp->flags & flags;
	if ((flags_rdy != 0) && consume)
		grp->flags |= flags_rdy;
	
	return flags_rdy;
}

flag_t flag_accept(struct flag_grp *grp, flag_t flags, int wait_type)
{
	unsigned long irq;
	flag_t flags_rdy;
	int result;
	int consume;

	result = wait_type & FLAG_CONSUME;
	if (result != 0) {
		wait_type &= ~FLAG_CONSUME;
		consume = 1;
	} else
		consume = 0;

	spin_lock_irqsave(&grp->lock, irq);

	switch (wait_type) {
	case FLAG_WAIT_SET_ALL:
		flags_rdy = flag_wait_set_all(grp, flags, consume);
		break;
	case FLAG_WAIT_SET_ANY:
		flags_rdy = flag_wait_set_any(grp, flags, consume);
		break;
	case FLAG_WAIT_CLR_ALL:
		flags_rdy = flag_wait_clr_all(grp, flags, consume);
		break;
	case FLAG_WAIT_CLR_ANY:
		flags_rdy = flag_wait_clr_any(grp, flags, consume);
		break;
	default:
		flags_rdy = 0;
		break;
	}

	spin_unlock_irqrestore(&grp->lock, irq);
	return flags_rdy;
}

static void flag_task_ready(struct flag_node *node, flag_t flags)
{
	struct task *task = node->task;

	__wake_up(task, TASK_STAT_PEND_OK, TASK_EVENT_FLAG,
			(void *)(unsigned long)flags);
}

static void flag_block(struct flag_grp *grp, struct flag_node *pnode,
		flag_t flags, int wait_type, uint32_t timeout)
{
	struct task *task = get_current_task();

	memset(pnode, 0, sizeof(struct flag_node));
	pnode->flags = flags;
	pnode->wait_type = wait_type;
	pnode->task = task;
	pnode->flag_grp = grp;
	list_add_tail(&grp->wait_list, &pnode->list);

	event_task_wait(pnode, TASK_EVENT_FLAG, timeout);
}

flag_t flag_pend(struct flag_grp *grp, flag_t flags,
		int wait_type, uint32_t timeout)
{
	unsigned long irq;
	struct flag_node node;
	flag_t flags_rdy = 0;
	int result, consume;
	struct task *task = get_current_task();

	might_sleep();

	result = wait_type & FLAG_CONSUME;
	if (result) {
		wait_type &= ~FLAG_CONSUME;
		consume = 1;
	} else {
		consume = 0;
	}

	spin_lock_irqsave(&grp->lock, irq);

	/*
	 * check the related flags is set or clear, if the
	 * condition is matched, then return. if the type is
	 * not support, the task will wait forever
	 */
	switch (wait_type) {
	case FLAG_WAIT_SET_ALL:
		flags_rdy = flag_wait_set_all(grp, flags, consume);
		break;
	case FLAG_WAIT_SET_ANY:
		flags_rdy = flag_wait_set_any(grp, flags, consume);
		break;
	case FLAG_WAIT_CLR_ALL:
		flags_rdy = flag_wait_clr_all(grp, flags, consume);
		break;
	case FLAG_WAIT_CLR_ANY:
		flags_rdy = flag_wait_clr_any(grp, flags, consume);
		break;
	default:
		flags_rdy = 0;
		break;
	}

	if (flags_rdy) {
		spin_unlock_irqrestore(&grp->lock, irq);
		return flags_rdy;
	}

	/*
	 * if the condition does not matched, then the task
	 * will suspend to wait the requested flags
	 */
	flag_block(grp, &node, flags, wait_type, timeout);
	spin_unlock_irqrestore(&grp->lock, irq);

	sched();

	spin_lock_irqsave(&grp->lock, irq);

	/*
	 * wait timeout or the releated event happened
	 */
	if (task->pend_stat != TASK_STAT_PEND_OK) {
		task->pend_stat = TASK_STAT_PEND_OK;
		list_del(&node.list);
		flags_rdy = 0;
	} else {
		flags_rdy = task->flags_rdy;
		if (consume) {
			switch (wait_type) {
			case FLAG_WAIT_SET_ALL:
			case FLAG_WAIT_SET_ANY:
				grp->flags &= ~flags_rdy;
				break;

			case FLAG_WAIT_CLR_ALL:
			case FLAG_WAIT_CLR_ANY:
				grp->flags |= flags_rdy;
				break;

			default:
				flags_rdy = 0;
				break;
			}
		}
	}

	spin_unlock_irqrestore(&grp->lock, irq);

	return flags_rdy;
}

flag_t flag_pend_get_flags_ready(void)
{
	return current->flags_rdy;
}

flag_t flag_post(struct flag_grp *grp, flag_t flags, int opt)
{
	flag_t flags_rdy;
	unsigned long irq;
	struct flag_node *pnode, *n;

	if (opt > FLAG_SET)
		return -EINVAL;

	spin_lock_irqsave(&grp->lock, irq);
	switch (opt) {
	case FLAG_CLR:
		grp->flags &= ~flags;
		break;
	
	case FLAG_SET:
		grp->flags |= flags;
		break;
	}

	list_for_each_entry_safe(pnode, n, &grp->wait_list, list) {
		switch (pnode->wait_type) {
		case FLAG_WAIT_SET_ALL:
			flags_rdy = grp->flags & pnode->flags;
			if (flags_rdy == pnode->flags)
				flag_task_ready(pnode, flags_rdy);
			break;

		case FLAG_WAIT_SET_ANY:
			flags_rdy = grp->flags & pnode->flags;
			if (flags_rdy != 0)
				flag_task_ready(pnode, flags_rdy);
			break;

		case FLAG_WAIT_CLR_ALL:
			flags_rdy = ~grp->flags & pnode->flags;
			if (flags_rdy == pnode->flags)
				flag_task_ready(pnode, flags_rdy);
			break;

		case FLAG_WAIT_CLR_ANY:
			flags_rdy = ~grp->flags & pnode->flags;
			if (flags_rdy != 0)
				flag_task_ready(pnode, flags_rdy);

		default:
			spin_unlock_irqrestore(&grp->lock, irq);
			return 0;
		}
	}

	spin_unlock_irqrestore(&grp->lock, irq);

	cond_resched();

	return grp->flags;
}
