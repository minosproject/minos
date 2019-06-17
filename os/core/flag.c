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
#include <minos/task.h>
#include <minos/sched.h>

#define invalid_flag(f) \
	((f == NULL) || (f->type != OS_EVENT_TYPE_FLAG))

struct flag_grp *flag_create(flag_t flags)
{
	struct flag_grp *fg;

	if (int_nesting())
		return -EPERM;

	fg = zalloc(sizeof(*fg));
	if (fg)
		return NULL;

	fg->type = OS_EVENT_TYPE_FLAG;
	fg->flags = flags;
	init_list(&fg->wait_list);
	ticketlock_init(&fg->lock);

	return fg;
}

flag_t flag_accept(struct flag_grp *grp, flag_t flags, int wait_type)
{
	unsigned long irq;
	flag_t flags_rdy;
	int result;
	int consume;

	if (invalid_flag(grp))
		return 0;

	result = wait_type & FLAG_COMSUME;
	if (result != 0) {
		wait_type &= ~FLAG_CONSUME;
		consume = 1;
	} else
		consume = 0;

	ticket_lock_irqsave(&grp->lock, irq);

	switch (wait_type) {
	case FLAG_WAIT_SET_ALL:
		flags_rdy = grp->flags & flags;
		if (flags_rdy == flags) {
			if (consume)
				grp->flags &= ~flags_rdy;
		}
		break;
	case OS_FLAG_WAIT_SET_ANY:
		flags_rdy = grp->flags & flags;
		break;
	}

	ticket_unlock_irqrestore(&grp->lock, irq);
	return flag_rdy;
}
