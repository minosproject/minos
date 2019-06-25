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

static void rt_task(void *data)
{
	while (1) {
		pr_info("rt_task test on cpu-%d\n", smp_processor_id());
		msleep(100);
	}
}

void apps_cpu0_init(void)
{
	create_realtime_task("rt-task-1", rt_task, NULL, 45, 4096, 0);
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
	while (1) {
		pr_info("test task 1 on %d\n", smp_processor_id());
		mdelay(100);
	}
}
DEFINE_TASK_PERCPU("test task", test_task, NULL,  4096, 0);

void test_task2(void *data)
{
	while (1) {
		pr_info("test task 2 on %d\n", smp_processor_id());
		mdelay(100);
	}
}
//DEFINE_TASK_PERCPU("test task2", test_task2, NULL,  4096, 0);
