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
#include <minos/mutex.h>

DEFINE_MUTEX(rt_mutex);

static void rt_task(void *data)
{
	int ret = 0;
	int task_id = (int)((unsigned long)data);

	while (1) {
		ret = mutex_pend(&rt_mutex, 100);
		if (ret) {
			pr_info("wait mutex timeout\n");
			continue;
		}
		pr_info("rt_task-%d test on cpu-%d\n",
				task_id, smp_processor_id());
		mdelay(100);
		mutex_post(&rt_mutex);
		msleep(100 * (task_id % 4 + 1));
	}
}

void os_init(void)
{
	mutex_init(&rt_mutex, "rt_mutex");
}

void apps_cpu0_init(void)
{
#if 0
	create_realtime_task("rt-task-45", rt_task, (void *)45, 45, 0);
	create_realtime_task("rt-task-44", rt_task, (void *)44, 44, 0);
	create_realtime_task("rt-task-43", rt_task, (void *)43, 43, 0);
	create_realtime_task("rt-task-42", rt_task, (void *)42, 42, 0);
	create_realtime_task("rt-task-41", rt_task, (void *)41, 41, 0);
	create_realtime_task("rt-task-40", rt_task, (void *)40, 40, 0);
	create_realtime_task("rt-task-39", rt_task, (void *)39, 39, 0);
	create_realtime_task("rt-task-38", rt_task, (void *)38, 38, 0);
	create_realtime_task("rt-task-37", rt_task, (void *)37, 37, 0);
	create_realtime_task("rt-task-36", rt_task, (void *)36, 36, 0);
	create_realtime_task("rt-task-35", rt_task, (void *)35, 35, 0);
	create_realtime_task("rt-task-34", rt_task, (void *)34, 34, 0);
	create_realtime_task("rt-task-33", rt_task, (void *)33, 33, 0);
	create_realtime_task("rt-task-32", rt_task, (void *)32, 32, 0);
	create_realtime_task("rt-task-31", rt_task, (void *)31, 31, 0);
	create_realtime_task("rt-task-30", rt_task, (void *)30, 30, 0);
	create_realtime_task("rt-task-29", rt_task, (void *)29, 29, 0);
	create_realtime_task("rt-task-28", rt_task, (void *)28, 28, 0);
	create_realtime_task("rt-task-27", rt_task, (void *)27, 27, 0);
	create_realtime_task("rt-task-26", rt_task, (void *)26, 26, 0);
	create_realtime_task("rt-task-25", rt_task, (void *)25, 25, 0);
	create_realtime_task("rt-task-24", rt_task, (void *)24, 24, 0);
	create_realtime_task("rt-task-23", rt_task, (void *)23, 23, 0);
	create_realtime_task("rt-task-22", rt_task, (void *)22, 22, 0);
	create_realtime_task("rt-task-21", rt_task, (void *)21, 21, 0);
	create_realtime_task("rt-task-20", rt_task, (void *)20, 20, 0);
	create_realtime_task("rt-task-19", rt_task, (void *)19, 19, 0);
	create_realtime_task("rt-task-18", rt_task, (void *)18, 18, 0);
	create_realtime_task("rt-task-17", rt_task, (void *)17, 17, 0);
#endif
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
	int ret;

	while (1) {
		ret = mutex_pend(&rt_mutex, 100);
		if (ret) {
			pr_err("timeout waitting for mutex\n");
			continue;
		}

		pr_info("test task 1 on %d\n", smp_processor_id());
		mdelay(100);
		mutex_post(&rt_mutex);

		msleep(100);
	}
}
//DEFINE_TASK_PERCPU("test task", test_task, NULL, 0);

void test_task2(void *data)
{
	int ret;

	while (1) {
		ret = mutex_pend(&rt_mutex, 100);
		if (ret) {
			pr_err("timeout waitting for mutex\n");
			continue;
		}

		pr_info("test task 2 on %d\n", smp_processor_id());
		mdelay(100);
		mutex_post(&rt_mutex);

		msleep(100);
	}
}
//DEFINE_TASK_PERCPU("test task2", test_task2, NULL, 0);

void test_task3(void *data)
{
	while (1) {
		pr_info("test task 3 on %d\n", smp_processor_id());
		mdelay(100);
		msleep(100);
	}
}
//DEFINE_TASK_PERCPU("test task3", test_task3, NULL, 0);
