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

#include <minos/types.h>
#include <minos/arch.h>
#include <minos/time.h>
#include <minos/task.h>
#include <minos/sched.h>

void udelay(uint32_t us)
{
	unsigned long deadline = get_sys_time() + 1000 *
		(unsigned long)us;

	while (get_sys_time() < deadline);

	dsbsy();
	isb();
}

void mdelay(uint32_t ms)
{
	unsigned long deadline = get_sys_time();

	deadline += 1000000 * (unsigned long)ms;
	while (get_sys_time() < deadline);

	dsbsy();
	isb();
}

void msleep(uint32_t ms)
{
	set_task_suspend(ms);
}
