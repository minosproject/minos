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
#include <minos/task.h>
#include <minos/sched.h>
#include <minos/mm.h>
#include <minos/atomic.h>

static DEFINE_SPIN_LOCK(pid_lock);
static DECLARE_BITMAP(pid_map, OS_NR_TASKS);
struct task *os_task_table[OS_NR_TASKS];

static atomic_t os_task_nr;

int alloc_pid(prio_t prio, int cpuid)
{
	int pid = -1;
	struct pcpu *pcpu = get_per_cpu(pcpu, cpuid);

	/*
	 * check whether this task is a global task or
	 * a task need to attach to the special pcpu and
	 * also check the whether the prio is valid or
	 * invalid. by the side the idle and stat task is
	 * created by the pcpu itself at the boot stage
	 */
	spin_lock(&pid_lock);

	if (prio > OS_LOWEST_PRIO) {
		if (prio == OS_PRIO_IDLE) {
			if (pcpu->idle_task)
				goto out;
		}

		pid = find_next_zero_bit(pid_map, OS_NR_TASKS,
				OS_REALTIME_TASK);
		if (pid >= OS_NR_TASKS)
			pid = -1;
	} else {
		if (!test_and_set_bit(prio, pid_map)) {
			pid = prio;
			os_task_table[pid] = OS_TASK_RESERVED;
		}
	}

out:
	spin_unlock(&pid_lock);

	return pid;
}

void release_pid(int pid)
{
	if (pid > OS_NR_TASKS)
		return;

	spin_lock(&pid_lock);
	clear_bit(pid, pid_map);
	os_task_table[pid] = NULL;
	spin_unlock(&pid_lock);
}

struct task *pid_to_task(int pid)
{
	if (pid >= OS_NR_TASKS)
		return NULL;

	return os_task_table[pid];
}

static void task_init(struct task *task, char *name,
		void *stack, void *arg, prio_t prio,
		int pid, int aff,size_t stk_size, unsigned long opt)
{
	if (!task || !stack)
		return;

	task->stack_base = task->stack_origin = stack;
	task->stack_size = stk_size;
	task->udata = arg;

	task->flags = opt;
	task->pid = pid;

	task->prio = prio;

	if (prio <= OS_LOWEST_PRIO) {
		task->by = prio >> 3;
		task->bx = prio & 0x07;
		task->bity = 1ul << task->by;
		task->bitx = 1ul << task->bx;
	}

	task->stat = TASK_STAT_RDY;
	task->pend_stat = 0;

	task->affinity = aff;
	task->flags = opt;
	task->del_req = 0;

	if (task->prio == OS_PRIO_IDLE)
		task->flags |= TASK_FLAGS_IDLE;

	strncpy(task->name, name, MIN(strlen(name), TASK_NAME_SIZE));
}

static struct task *__create_task(char *name, task_func_t func,
		void *arg, prio_t prio, int pid, int aff,
		size_t stk_size, unsigned long opt)
{
	struct task *task;
	void *stack = NULL;
	struct pcpu *pcpu;

	/* now create the task and init it */
	task = zalloc(sizeof(*task));
	if (!task) {
		pr_error("no more memory for task\n");
		return NULL;
	}

	/* allocate the stack for this task */
	if (stk_size) {
#ifdef CONFIG_STACK_PAGE_ALIGN
		stk_size = BALIGN(stk_size, PAGE_SIZE);
		stack = get_free_pages(PAGE_NR(stk_size));
#else
		stk_size = BALIGN(stk_size, sizeof(unsigned long));
		stack = malloc(stk_size);
#endif
		if (stack == NULL) {
			pr_error("no more memory for task stack\n");
			free(task);
			return NULL;
		}
	}

	/* store this task to the task table */
	os_task_table[pid] = task;
	atomic_inc(&os_task_nr);

	if ((task->affinity < NR_CPUS) && (prio == OS_PRIO_PCPU)) {
		pcpu = get_per_cpu(pcpu, task->affinity);
		spin_lock(&pcpu->lock);
		list_add_tail(&pcpu->task_list, &task->list);
		spin_unlock(&pcpu->lock);
	}

	task_init(task, name, stack, arg, prio,
			pid, aff, stk_size, opt);

	return task;
}

void task_create_hook(struct task *task)
{

}

int create_task(char *name, task_func_t func,
		void *arg, prio_t prio, uint16_t aff,
		uint32_t stk_size, uint32_t opt)
{
	int pid = -1;
	struct task *task;

	if (int_nesting())
		return -EFAULT;

	if ((aff >= NR_CPUS) && (aff != PCPU_AFF_NONE))
		return

	pid = alloc_pid(prio, aff);
	if (pid < 0)
		return -ENOPID;

	task = __create_task(name, func, arg, prio,
			pid, aff, stk_size, opt);
	if (!task) {
		release_pid(pid);
		pid = -ENOPID;
	}

	/*
	 * the vcpu task's stat is different with the normal
	 * task
	 */
	if (!(task->flags & TASK_FLAGS_VCPU)) {
		set_task_ready(task);

		if (os_is_running())
			sched();
	}

	task_create_hook(task);
	arch_init_task(task, (void *)func, task->udata);

	return pid;
}

int create_idle_task(void)
{
	int pid;
	struct task *task;
	int aff = smp_processor_id();
	extern unsigned char __el2_stack_end;
	void *el2_stack_base = (void *)&__el2_stack_end;

	pid = alloc_pid(OS_PRIO_IDLE, aff);

	task = __create_task("idle", NULL, NULL,
			OS_PRIO_IDLE, pid, aff, 0, 0);
	if (!task)
		panic("can not create idle task for pcpu%d\n", aff);

	task->stack_size = TASK_DEFAULT_STACK_SIZE;
	task->stack_origin = el2_stack_base -
		(aff << CONFIG_IDLE_TASK_STACK_SHIFT);
	task->stat = TASK_STAT_RUNNING;

	set_current_task(task);
	set_next_task(task);

	set_current_prio(OS_PRIO_IDLE);
	set_next_prio(OS_PRIO_IDLE);

	return 0;
}
