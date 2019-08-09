/*
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
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
#include <minos/task.h>
#include <minos/sched.h>
#include <minos/irq.h>

DEFINE_SPIN_LOCK(__kernel_lock);

void __might_sleep(const char *file, int line, int preempt_offset)
{
	WARN_ONCE(current->stat != TASK_STAT_RUNNING,
			"do not call blocking ops when !TASK_RUNNING; "
			"state=%d", current->stat);

	if (preempt_allowed() && !irq_disabled() && !is_idle_task(current))
		return;

	pr_err("BUG: sleeping function called from invalid context at %s:%d\n",
			file, line);
	dump_stack(NULL, (unsigned long *)arch_get_sp());
}
