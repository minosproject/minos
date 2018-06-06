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
#include <minos/bitops.h>
#include <minos/smp.h>
#include <minos/sched.h>
#include <minos/arch.h>

static int get_default_affinity(void)
{
	return 0;
}

static uint64_t alloc_pid(void)
{
	static uint64_t minos_pid = 0;

	return minos_pid++;
}

static struct task *__create_task(char *name,
		void *stack_base, uint32_t stack_size, int pr,
		int affinity, void *p, unsigned long flag)
{
	struct task *task;

	task = (struct task *)malloc(sizeof(struct task));
	if (!task) {
		pr_error("No more memory for task\n");
		return NULL;
	}

	memset((char *)task, 0, sizeof(struct task));

	task->stack_base = stack_base + stack_size;
	task->stack_size = stack_size;
	task->pid = alloc_pid();
	task->affinity = affinity;
	task->state = TASK_STAT_SUSPEND;
	task->pend_state = 0;
	task->task_type = TASK_TYPE_NORMAL;
	task->is_idle = 0;

	init_list(&task->list);

	strncpy(task->name, name,
		strlen(name) > (TASK_NAME_SIZE - 1) ?
		(TASK_NAME_SIZE - 1) : strlen(name));
	task->pdata = p;

	return task;
}

struct task *create_task(char *name, void *entry,
		uint32_t stack_size, int pr, int affinity,
		void *p, unsigned long flag)
{
	struct task *task;
	void *stack_base;

	if (!entry)
		return NULL;

	stack_size = BALIGN(stack_size, SIZE_4K);
	stack_base = (void *)get_free_pages(PAGE_NR(stack_size));
	if (!stack_base) {
		pr_error("No more memory for task stack\n");
		return NULL;
	}

	if (stack_size == 0)
		stack_size = TASK_DEFAULT_STACK_SIZE;

	if ((affinity == -1) || (affinity >= NR_CPUS))
		affinity = get_default_affinity();

	task = __create_task(name, stack_base,
			stack_size, pr, affinity, p, flag);
	if (!task)
		return NULL;

	/* set and init the stack */
	arch_init_task(task, entry);
	pcpu_add_task(affinity, task);

	/*
	 * if the task is a vcpu, do not set it to ready
	 * after create it, since vcpu task have some boot
	 * protocl in each os
	 */
	if (!(flag & TASK_FLAG_VCPU))
		set_task_ready(task);

	return task;
}

struct task *create_idle_task(void)
{
	struct task *idle;
	int cpu = smp_processor_id();

	idle = __create_task("idle", 0, TASK_DEFAULT_STACK_SIZE,
			TASK_IDLE_PR, cpu, NULL, 0);
	if (!idle)
		panic("Can not create idle task\n");

	idle->stack_base = 0;
	idle->is_idle = 1;
	pcpu_add_task(cpu, idle);
	set_task_ready(idle);
	idle->state = TASK_STAT_RUNNING;

	current_task = idle;
	next_task = idle;

	return idle;
}

void tasks_init(void)
{

}
