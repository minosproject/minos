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
#include <minos/vmodule.h>

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
		else
			set_bit(pid, pid_map);
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

static void task_timeout_handler(unsigned long data)
{
	struct task *task = (struct task *)data;

	/*
	 * when task is suspended by sleep or waitting
	 * for a event, it may set the delay time, when
	 * the delay time is arrvie, then it will called
	 * this function
	 */
	task_lock(task);

	if (task->delay) {
		/* task is timeout and check its stat */
		task->delay = 0;

		if (is_task_pending(task)) {
			task->stat &= ~TASK_STAT_PEND_ANY;
			task->pend_stat = TASK_STAT_PEND_TO;
		} else
			task->pend_stat = TASK_STAT_PEND_OK;

		if (!is_task_ready(task)) {
			set_task_ready(task);
			task->stat &= TASK_STAT_RDY;
		}
	} else {
		pr_warn("Wrong task stat stat-%d pend-stat-%d\n",
				task->stat, task->pend_stat);
	}

	pcpu_need_resched();
	task_unlock(task);
}

static void task_init(struct task *task, char *name,
		void *stack, void *arg, prio_t prio,
		int pid, int aff,size_t stk_size, unsigned long opt)
{
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

	task->pend_stat = 0;
	if (task->flags & TASK_FLAGS_VCPU)
		task->stat = TASK_STAT_SUSPEND;
	else
		task->stat = TASK_STAT_RDY;

	task->affinity = aff;
	task->flags = opt;
	task->del_req = 0;
	task->run_time = CONFIG_TASK_RUN_TIME;

	if (task->prio == OS_PRIO_IDLE)
		task->flags |= TASK_FLAGS_IDLE;

	init_timer_on_cpu(&task->delay_timer, aff);
	task->delay_timer.function = task_timeout_handler;
	task->delay_timer.data = (unsigned long)task;
	strncpy(task->name, name, MIN(strlen(name), TASK_NAME_SIZE));
}

static struct task *__create_task(char *name, task_func_t func,
		void *arg, prio_t prio, int pid, int aff,
		size_t stk_size, unsigned long opt)
{
	struct task *task;
	void *stack = NULL;

	/* now create the task and init it */
	task = zalloc(sizeof(*task));
	if (!task) {
		pr_err("no more memory for task\n");
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
			pr_err("no more memory for task stack\n");
			free(task);
			return NULL;
		} else {
			pr_info("stack 0x%x for task-%d\n",
					(unsigned long)stack, pid);
		}
	}

	/* store this task to the task table */
	os_task_table[pid] = task;
	atomic_inc(&os_task_nr);

	if (aff == PCPU_AFF_NONE)
		aff = 0;
	else if (aff == PCPU_AFF_PERCPU)
		aff = smp_processor_id();

	task_init(task, name, stack, arg, prio,
			pid, aff, stk_size, opt);
	task_vmodules_init(task);

	return task;
}

static void task_create_hook(struct task *task)
{
	do_hooks((void *)task, NULL, OS_HOOK_CREATE_TASK);
}

static void task_ipi_event_handler(void *data)
{
	struct task *task;
	struct task_event *ev = (struct task_event *)data;

	if (data == NULL)
		pr_err("got invalid argument in %s\n", __func__);

	task = ev->task;
	if (task->affinity != smp_processor_id())
		return;

	switch (ev->action) {
	case TASK_EVENT_EVENT_READY:
		/* if the task has been timeout then skip it */
		if (!is_task_pending(task))
			return;

		task->delay = 0;
		task->msg = ev->msg;
		task->stat &= ~ev->msk;
		task->pend_stat = ev->pend_stat;
		task->wait_event = NULL;

		set_task_ready(task);
		break;

	case TASK_EVENT_FLAG_READY:
		if (!is_task_pending(task))
			return;

		task->delay = 0;
		task->flags_rdy = ev->flags;
		task->stat &= ev->msk;
		task->pend_stat = ev->pend_stat;
		break;

	default:
		break;
	}

	/* set resched flag according to the current prio
	 * BUG - Do not free memory in interrupt, need to
	 * fix it
	 */
	free(ev);
	pcpu_need_resched();
}

int task_ipi_event(struct task *task, struct task_event *ev, int wait)
{
	return smp_function_call(task->affinity, (void *)ev,
			task_ipi_event_handler, wait);
}

int create_task(char *name, task_func_t func,
		void *arg, prio_t prio, uint16_t aff,
		uint32_t stk_size, unsigned long opt)
{
	int pid = -1;
	struct task *task;
	unsigned long flags;
	struct pcpu *pcpu;

	if (int_nesting())
		return -EFAULT;

	if ((aff >= NR_CPUS) && (aff != PCPU_AFF_NONE))
		return -EINVAL;

	pid = alloc_pid(prio, aff);
	if (pid < 0)
		return -ENOPID;

	task = __create_task(name, func, arg, prio,
			pid, aff, stk_size, opt);
	if (!task) {
		release_pid(pid);
		pid = -ENOPID;
	}

	task_create_hook(task);
	arch_init_task(task, (void *)func, task->udata);

	aff = task->affinity;
	pcpu = get_per_cpu(pcpu, aff);

	/*
	 * after create the task, if the task is affinity to
	 * the current cpu, then it can add the task to
	 * the ready list directly, this action need done after
	 * the task has been finish all the related init things
	 */
	if ((aff < NR_CPUS) && (prio == OS_PRIO_PCPU)) {
		pcpu = get_per_cpu(pcpu, aff);
		spin_lock_irqsave(&pcpu->lock, flags);
		list_add_tail(&pcpu->task_list, &task->list);
		if (aff == smp_processor_id())
			list_add_tail(&pcpu->ready_list, &task->stat_list);
		else
			list_add_tail(&pcpu->new_list, &task->stat_list);
		pcpu->nr_pcpu_task++;
		spin_unlock_irqrestore(&pcpu->lock, flags);
	}

	/*
	 * the vcpu task's stat is different with the normal
	 * task, the vcpu task's init stat is controled by
	 * other mechism
	 */
	if (!(task->flags & TASK_FLAGS_VCPU)) {
		/*
		 * percpu task has already added the the
		 * ready list
		 */
		if (is_realtime_task(task)) {
			kernel_lock_irqsave(flags);
			set_task_ready(task);
			kernel_unlock_irqrestore(flags);
		}

		/*
		 * if the task is a realtime task and the os
		 * sched is running then resched the task
		 * otherwise send a ipi to the task
		 */
		if (os_is_running()) {
			if (is_realtime_task(task))
				sched();
			else {
				if (aff != smp_processor_id())
					pcpu_resched(aff);
			}
		}
	}

	return pid;
}

int create_idle_task(void)
{
	int pid;
	struct task *task;
	int aff = smp_processor_id();
	extern unsigned char __el2_stack_end;
	void *el2_stack_base = (void *)&__el2_stack_end;
	struct pcpu *pcpu = get_per_cpu(pcpu, aff);

	pid = alloc_pid(OS_PRIO_IDLE, aff);
	if (pid < -1)
		panic("can not create task, PID error\n");

	task = __create_task("idle", NULL, NULL,
			OS_PRIO_IDLE, pid, aff, 0, 0);
	if (!task)
		panic("can not create idle task for pcpu%d\n", aff);

	task->stack_size = TASK_DEFAULT_STACK_SIZE;
	task->stack_origin = el2_stack_base -
		(aff << CONFIG_IDLE_TASK_STACK_SHIFT);
	task->stat = TASK_STAT_RUNNING;
	task->flags |= TASK_FLAGS_IDLE;

	pcpu->idle_task = task;

	/* call the hooks for the idle task */
	task_create_hook(task);

	set_current_task(task);
	set_next_task(task);

	set_current_prio(OS_PRIO_PCPU);
	set_next_prio(OS_PRIO_PCPU);

	return 0;
}

int create_percpu_task(char *name, task_func_t func, void *arg,
		size_t stk_size, unsigned long flags)
{
	int cpu, ret = 0;

	for_each_online_cpu(cpu) {
		ret = create_task(name, func, arg, OS_PRIO_PCPU,
				cpu, stk_size, flags);
		if (ret < 0)
			pr_err("create [%s] fail on cpu%d\n", name, cpu);
	}

	return 0;
}

int create_realtime_task(char *name, task_func_t func, void *arg,
		prio_t prio, size_t stk_size, unsigned long flags)
{
	return create_task(name, func, arg, prio, 0, stk_size, flags);
}

int create_vcpu_task(char *name, task_func_t func, void *arg,
		int aff, size_t stk_size, unsigned long flags)
{
	return create_task(name, func, arg, OS_PRIO_PCPU,
			aff, stk_size, flags & TASK_FLAGS_VCPU);
}
