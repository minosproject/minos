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

#define invalid_flag(f) \
	((f == NULL) || (f->type != OS_EVENT_TYPE_FLAG))

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
	if (flags_rdy != 0) {
		if (consume)
			grp->flags &= ~flags_rdy;
	} else
		flags_rdy = 0;

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
	if (flags_rdy != 0) {
		if (consume)
			grp->flags |= flags_rdy;
	} else
		flags_rdy = 0;

	return flags_rdy;
}

flag_t flag_accept(struct flag_grp *grp, flag_t flags, int wait_type)
{
	unsigned long irq;
	flag_t flags_rdy;
	int result;
	int consume;

	if (invalid_flag(grp))
		return 0;

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

static int flag_task_ready(struct flag_node *node, flag_t flags)
{
	int sched = 0;
#if 0
	struct task *task;
	int cpuid = smp_processor_id();
	struct task_event *tevent;

	task = node->task;

	if (task_is_realtime(task) || (task->affinity == cpuid)) {
		task_lock(task);
		task->delay = 0;
		task->flags_rdy = flags;
		task->stat &= ~TASK_STAT_FLAG;
		task->pend_stat = TASK_STAT_PEND_OK;
		if (task->stat == TASK_STAT_RDY)
			sched = 1;
		else
			sched = 0;

		task_unlock(task);
	} else {
		tevent = zalloc(sizeof(*tevent));
		if (!tevent)
			panic("no memory for flag task event\n");

		tevent->msk = TASK_STAT_FLAG;
		tevent->task = task;
		tevent->action = TASK_EVENT_FLAG_READY;
		tevent->flags = flags;

		task_ipi_event(task, tevent, 0);
	}
#endif
	return sched;
}

static void flag_block(struct flag_grp *grp, struct flag_node *pnode,
		flag_t flags, int wait_type, uint32_t timeout)
{
	struct task *task = get_current_task();

	memset(pnode, 0, sizeof(*pnode));
	pnode->flags = flags;
	pnode->wait_type = wait_type;
	pnode->task = task;
	pnode->flag_grp = grp;

	task_lock(task);
	task->stat |= TASK_STAT_FLAG;
	task->pend_stat = TASK_STAT_PEND_OK;
	task->delay = timeout;
	task->flag_node = pnode;
	list_add_tail(&grp->wait_list, &pnode->list);

	set_task_sleep(task, timeout);

	task_unlock(task);
}

flag_t flag_pend(struct flag_grp *grp, flag_t flags,
		int wait_type, uint32_t timeout)
{
	unsigned long irq;
	struct flag_node node;
	flag_t flags_rdy;
	int result, consume;
	struct task *task = get_current_task();

	if (invalid_flag(grp))
		return 0;

	might_sleep();

	result = wait_type & FLAG_CONSUME;
	if (result) {
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

	if (flags_rdy) {
		spin_unlock_irqrestore(&grp->lock, irq);
		return flags_rdy;
	}

	flag_block(grp, &node, flags, wait_type, timeout);
	spin_unlock_irqrestore(&grp->lock, irq);

	sched();

	spin_lock_irqsave(&grp->lock, irq);
	task_lock(task);

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

	task_unlock(task);
	spin_unlock_irqrestore(&grp->lock, irq);

	return flags_rdy;
}

flag_t flag_pend_get_flags_ready(void)
{
	struct task *task = get_current_task();
	unsigned long irq;
	flag_t flags;

	/* TBD */
	spin_lock_irqsave(&task->lock, irq);
	flags = task->flags_rdy;
	spin_unlock_irqrestore(&task->lock, irq);

	return flags;
}

flag_t flag_post(struct flag_grp *grp, flag_t flags, int opt)
{
	int need_sched;
	flag_t flags_rdy;
	unsigned long irq;
	struct flag_node *pnode, *n;

	if (invalid_flag(grp) || (opt > FLAG_SET))
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

	need_sched = 0;
	list_for_each_entry_safe(pnode, n, &grp->wait_list, list) {
		switch (pnode->wait_type) {
		case FLAG_WAIT_SET_ALL:
			flags_rdy = grp->flags & pnode->flags;
			if (flags_rdy == pnode->flags) {
				need_sched = flag_task_ready(pnode,
						flags_rdy);
			}
			break;

		case FLAG_WAIT_SET_ANY:
			flags_rdy = grp->flags & pnode->flags;
			if (flags_rdy != 0) {
				need_sched = flag_task_ready(pnode,
						flags_rdy);
			}
			break;

		case FLAG_WAIT_CLR_ALL:
			flags_rdy = ~grp->flags & pnode->flags;
			if (flags_rdy == pnode->flags) {
				need_sched = flag_task_ready(pnode,
						flags_rdy);
			}
			break;

		case FLAG_WAIT_CLR_ANY:
			flags_rdy = ~grp->flags & pnode->flags;
			if (flags_rdy != 0) {
				need_sched = flag_task_ready(pnode,
						flags_rdy);
			}

		default:
			spin_unlock_irqrestore(&grp->lock, irq);
			return 0;
		}
	}

	spin_unlock_irqrestore(&grp->lock, irq);

	if (need_sched)
		sched();

	spin_lock_irqsave(&grp->lock, irq);
	flags_rdy = grp->flags;
	spin_unlock_irqrestore(&grp->lock, irq);

	return flags_rdy;
}
