#include <minos/minos.h>
#include <minos/task.h>
#include <minos/bitops.h>
#include <minos/smp.h>
#include <minos/sched.h>
#include <minos/arch.h>

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
	task->pr = pr;
	task->affinity = affinity;
	atomic_set(&task->task_stat, TASK_STAT_IDLE);
	atomic_set(&task->task_pend_stat, TASK_STAT_PEND_OK);
	task->task_type = TASK_TYPE_NORMAL;

	task->bit_map_x = pr / 8;
	task->bit_map_y = pr % 8;

	init_list(&task->list);

	strncpy(task->name, name,
		strlen(name) > (TASK_NAME_SIZE - 1) ?
		(TASK_NAME_SIZE - 1) : strlen(name));
	task->pdata = p;

	return task;
}

static int get_default_affinity(void)
{
	return 0;
}

static int get_default_priority(int affinity)
{
	return 0;
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

	if (pr == -1)
		pr = get_default_priority(affinity);

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
	pcpu_add_task(cpu, idle);

	current_task = idle;
	next_task = idle;

	return idle;
}

void tasks_init(void)
{

}
