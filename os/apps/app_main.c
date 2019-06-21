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
#include <minos/time.h>

static void create_static_tasks(void)
{
	int ret = 0, cpu;
	struct task_desc *tdesc;
	extern unsigned char __task_desc_start;
	extern unsigned char __task_desc_end;

	section_for_each_item(__task_desc_start, __task_desc_end, tdesc) {
		if (tdesc->aff == PCPU_AFF_PERCPU) {
			for_each_online_cpu(cpu) {
				ret = create_task(tdesc->name, tdesc->func,
						tdesc->arg, OS_PRIO_PCPU,
						cpu, tdesc->stk_size,
						tdesc->flags);
				if(ret) {
					pr_err("create [%s] fail on cpu-%d\n",
							tdesc->name, cpu);
				}
			}
		} else {
			ret = create_task(tdesc->name, tdesc->func,
					tdesc->arg, tdesc->prio,
					tdesc->aff, tdesc->stk_size,
					tdesc->flags);
			if (ret) {
				pr_err("create [%s] fail on cpu-%d@%d\n",
					tdesc->name, tdesc->aff, tdesc->prio);
			}

		}
	}
}

void apps_cpu0_init(void)
{
	/* init the system start */
	/* init the system end */

	create_static_tasks();
}

void apps_cpu1_init(void)
{

}

void apps_cpu2_init(void)
{

}

void apps_cpu3_init(void)
{

}

void apps_cpu4_init(void)
{

}

void apps_cpu5_init(void)
{

}

void apps_cpu6_init(void)
{

}

void apps_cpu7_init(void)
{

}

void test_task(void *data)
{
	pr_info("test task\n");
	mdelay(100);
}
DEFINE_TASK(test, test_task, NULL, 20, 0, 4096, 0);
