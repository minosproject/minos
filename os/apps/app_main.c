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
	int task_id = (int)((unsigned long)data);

	while (1) {
		pr_info("rt_task-%d test on cpu-%d\n",
				task_id, smp_processor_id());
		mdelay(100);
		msleep(100 * (task_id % 4 + 1));
	}
}

void apps_cpu0_init(void)
{
	create_realtime_task("rt-task-45", rt_task, (void *)0, 45, 4096, 0);
	create_realtime_task("rt-task-44", rt_task, (void *)1, 44, 4096, 0);
	create_realtime_task("rt-task-43", rt_task, (void *)2, 43, 4096, 0);
	create_realtime_task("rt-task-42", rt_task, (void *)3, 42, 4096, 0);
	create_realtime_task("rt-task-41", rt_task, (void *)4, 41, 4096, 0);
	create_realtime_task("rt-task-40", rt_task, (void *)5, 40, 4096, 0);
	create_realtime_task("rt-task-39", rt_task, (void *)6, 39, 4096, 0);
	create_realtime_task("rt-task-38", rt_task, (void *)7, 38, 4096, 0);
	create_realtime_task("rt-task-37", rt_task, (void *)8, 37, 4096, 0);
	create_realtime_task("rt-task-36", rt_task, (void *)9, 36, 4096, 0);
	create_realtime_task("rt-task-35", rt_task, (void *)10, 35, 4096, 0);
	create_realtime_task("rt-task-34", rt_task, (void *)11, 34, 4096, 0);
	create_realtime_task("rt-task-33", rt_task, (void *)12, 33, 4096, 0);
	create_realtime_task("rt-task-32", rt_task, (void *)13, 32, 4096, 0);
	create_realtime_task("rt-task-31", rt_task, (void *)14, 31, 4096, 0);
	create_realtime_task("rt-task-30", rt_task, (void *)15, 30, 4096, 0);
	create_realtime_task("rt-task-29", rt_task, (void *)16, 29, 4096, 0);
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
DEFINE_TASK_PERCPU("test task2", test_task2, NULL,  4096, 0);
