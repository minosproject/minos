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
#include <minos/sched.h>
#include <minos/mm.h>
#include <minos/atomic.h>
#include <minos/task.h>

#ifdef CONFIG_VIRT
#include <virt/vmodule.h>
#endif

static DEFINE_SPIN_LOCK(pid_lock);
static DECLARE_BITMAP(pid_map, OS_NR_TASKS);
struct task *os_task_table[OS_NR_TASKS];

static atomic_t os_task_nr;

/* idle task needed be static defined */
static struct task idle_tasks[NR_CPUS];
static DEFINE_PER_CPU(struct task *, idle_task);

#define TASK_INFO_INIT(__ti, task, c) \
	do {		\
		__ti->cpu = c; \
		__ti->task = task; \
		__ti->preempt_count = 0; \
		__ti->flags = 0; \
	} while (0)

static int alloc_pid(uint8_t prio, int cpuid)
{
	int pid = -1;

	/*
	 * check whether this task is a global task or
	 * a task need to attach to the special pcpu and
	 * also check the whether the prio is valid or
	 * invalid. by the side the idle and stat task is
	 * created by the pcpu itself at the boot stage
	 */
	spin_lock(&pid_lock);

	if (prio > OS_LOWEST_REALTIME_PRIO) {
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

	spin_unlock(&pid_lock);

	return pid;
}

static void release_pid(int pid)
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

	if (task_is_pending(task)) {
		/* task is timeout and check its stat */
		task->delay = 0;

		task->stat &= ~TASK_STAT_SUSPEND;
		task->stat &= ~TASK_STAT_PEND_ANY;
		task->pend_stat = TASK_STAT_PEND_TO;
		set_task_ready(task, 0);
	} else {
		if ((task->delay) && !task_is_ready(task)) {
			task->delay = 0;
			task->stat &= ~TASK_STAT_SUSPEND;
			set_task_ready(task, 0);
		}
	}

	set_need_resched();
	task_unlock(task);
}

static void task_init(struct task *task, char *name,
		void *stack, void *arg, uint8_t prio,
		int pid, int aff,size_t stk_size, unsigned long opt)
{
	struct task_info *ti;

	if (stack) {
		stack += stk_size;
		task->stack_origin = stack - sizeof(struct task_info);
		task->stack_base = task->stack_origin;
		task->stack_size = stk_size;

		/* init the thread_info */
		ti = (struct task_info *)task->stack_origin;
		TASK_INFO_INIT(ti, task, aff);
	}

	task->udata = arg;
	task->flags = opt;
	task->pid = pid;
	task->prio = prio;

	if (prio <= OS_LOWEST_REALTIME_PRIO) {
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

	if (aff == PCPU_AFF_ANY)
		aff = select_task_run_cpu();
	
	task->affinity = aff;
	task->flags = opt;

	if (task->prio > OS_LOWEST_REALTIME_PRIO) {
		task->local_prio = task->prio - OS_REALTIME_TASK;
		task->local_mask = BIT(task->local_prio);
		task->run_time = CONFIG_TASK_RUN_TIME;
	}

	spin_lock_init(&task->lock);

	init_timer_on_cpu(&task->delay_timer, aff);
	task->delay_timer.function = task_timeout_handler;
	task->delay_timer.data = (unsigned long)task;
	strncpy(task->name, name, MIN(strlen(name), TASK_NAME_SIZE));
}

static struct task *__create_task(char *name, task_func_t func,
		void *arg, uint8_t prio, int pid, int aff,
		size_t stk_size, unsigned long opt)
{
	struct task *task;
	void *stack = NULL;

	/* now create the task and init it */
	task = zalloc(sizeof(struct task));
	if (!task) {
		pr_err("no more memory for task\n");
		return NULL;
	}

	/*
	 * if CONFIG_VIRT is enabled, system will use sp reg
	 * to get the task_info of the task, so need to keep
	 * the stack size as same in each task
	 */
#ifdef CONFIG_VIRT
	stk_size = TASK_STACK_SIZE;
	stack = __get_free_pages(PAGE_NR(stk_size), PAGE_NR(stk_size));
#else
	stk_size = BALIGN(stk_size, sizeof(unsigned long));
	if (stk_size)
		stack = malloc(stk_size);
#endif

	if (stk_size && !stack) {
		pr_err("no more memory for task stack\n");
		free(task);
		return NULL;
	} else {
		pr_debug("stack 0x%x for task-%d\n",
				(unsigned long)stack, pid);
	}

	/* store this task to the task table */
	os_task_table[pid] = task;
	atomic_inc(&os_task_nr);

	task_init(task, name, stack, arg, prio,
			pid, aff, stk_size, opt);

	return task;
}

static void task_create_hook(struct task *task)
{
	do_hooks((void *)task, NULL, OS_HOOK_CREATE_TASK);
}

void do_release_task(struct task *task)
{
	pr_notice("release task pid: %d name: %s\n",
			task->pid, task->name);
	/*
	 * this function can not be called at interrupt
	 * context, use release_task is more safe
	 */
	release_pid(task->pid);
	atomic_dec(&os_task_nr);

#ifdef CONFIG_VIRT
	if (task_is_vcpu(task))
		vcpu_vmodules_deinit(task->pdata);
#endif
	arch_release_task(task);

	free((task->stack_origin - task->stack_size + TASK_INFO_SIZE));
	free(task);
}

void release_task(struct task *task)
{
	unsigned long flags;
	struct pcpu *pcpu = get_cpu_var(pcpu);

	/*
	 * real time task and percpu time all link to
	 * the stop list, and delete from the pcpu global
	 * list, this function will always run ont the current
	 * cpu, mask the idle_block_flag to avoid cpu enter into
	 * idle state
	 */
	spin_lock_irqsave(&pcpu->lock, flags);
	if (task_is_percpu(task))
		list_del(&task->list);
	list_add_tail(&pcpu->stop_list, &task->list);
	spin_unlock_irqrestore(&pcpu->lock, flags);
}

void task_exit(int result)
{
	struct task *task = current;

	preempt_disable();

	/*
	 * set the task to stop stat, then call release_task
	 * to release the task
	 */
	set_task_sleep(task, 0);
	task->stat = TASK_STAT_STOPPED;

	/*
	 * to free the resource which the task obtain and releas
	 * it in case to block other task
	 */
	release_task(task);

	preempt_enable();

	/*
	 * nerver return after sched()
	 */
	sched();
	pr_fatal("task exit failed should not be here\n");

	while (1);
}

struct task *create_task(char *name, task_func_t func,
		void *arg, uint8_t prio, uint16_t aff,
		size_t stk_size, unsigned long opt)
{
	int pid = -1;
	struct task *task;
	unsigned long flags;
	struct pcpu *pcpu;

	if ((aff >= NR_CPUS) && (aff != PCPU_AFF_LOCAL) &&
			(aff != PCPU_AFF_ANY))
		return NULL;

	if (prio >= OS_PRIO_IDLE) {
		pr_err("invalid prio for task: %d\n", prio);
		return NULL;
	}

	if (aff == PCPU_AFF_LOCAL) {
		preempt_disable();
		aff = smp_processor_id();
		preempt_disable();
	}

	pid = alloc_pid(prio, aff);
	if (pid < 0)
		return NULL;

	if (prio <= OS_LOWEST_REALTIME_PRIO)
		opt |= TASK_FLAGS_REALTIME;

	task = __create_task(name, func, arg, prio,
			pid, aff, stk_size, opt);
	if (!task) {
		release_pid(pid);
		return NULL;
	}

	task_create_hook(task);
	arch_init_task(task, (void *)func, task->udata);

	aff = task->affinity;

	/* 
	 * after create the task, if the task is affinity to
	 * the current cpu, then it can add the task to
	 * the ready list directly, this action need done after
	 * the task has been finish all the related init things
	 */
	if (task_is_percpu(task)) {
		local_irq_save(flags);
		pcpu = get_per_cpu(pcpu, aff);

		raw_spin_lock(&pcpu->lock);
		list_add_tail(&pcpu->task_list, &task->list);

		if (task->stat == TASK_STAT_RDY) {
			if (aff == smp_processor_id()) {
				add_task_to_ready_list_tail(pcpu, task);
				pcpu->local_rdy_grp |= task->local_mask;
			} else {
				list_add_tail(&pcpu->new_list, &task->stat_list);
			}
		}

		pcpu->nr_pcpu_task++;
		raw_spin_unlock(&pcpu->lock);
		local_irq_restore(flags);
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
		if (task_is_realtime(task)) {
			kernel_lock_irqsave(flags);
			set_task_ready(task, 0);
			kernel_unlock_irqrestore(flags);
			set_need_resched();
		}

		/* 
		 * if the task is a realtime task and the os
		 * sched is running then resched the task
		 * otherwise send a ipi to the task
		 *
		 * if the os is not runing, may be need
		 * to send a ipi to other cpu which may aready
		 * ready to sched task
		 */
		if (task_is_realtime(task)) {
			if (os_is_running())
				sched();
			else
				cpus_resched();
		} else {
			if (aff != smp_processor_id()) {
				pcpu_irqwork(aff);
			} else if (task->local_prio < current->local_prio) {
				set_need_resched();
				sched_yield();
			}
		}
	}
	
	return task;
}

int create_idle_task(void)
{
	int pid;
	struct task *task;
	char task_name[32];
	int aff = smp_processor_id();
	struct pcpu *pcpu = get_per_cpu(pcpu, aff);

	task = get_cpu_var(idle_task);
	pid = alloc_pid(OS_PRIO_IDLE, aff);

	os_task_table[pid] = task;
	atomic_inc(&os_task_nr);
	sprintf(task_name, "idle@%d", aff);

	task_init(task, task_name, NULL, NULL,
			OS_PRIO_IDLE, pid, aff, 0, 0);

	/* reinit the task's stack information */
	task->stack_size = TASK_STACK_SIZE;
#ifndef CONFIG_VIRT
	task->stack_origin = (void *)current_sp() -
		sizeof(struct task_info);
#else
	task->stack_origin = (void *)current_task_info();
#endif

	task->stat = TASK_STAT_RUNNING;
	task->flags |= TASK_FLAGS_IDLE;
	task->run_time = 0;

	pcpu->running_task = task;

	/* call the hooks for the idle task */
	task_create_hook(task);

	set_current_prio(OS_PRIO_PCPU);
	set_next_prio(OS_PRIO_PCPU);

	add_task_to_ready_list(pcpu, task);
	pcpu->local_rdy_grp |= task->local_mask;
	pcpu->idle_task = task;

	return 0;
}

void os_for_all_task(void (*hdl)(struct task *task))
{
	int i;
	struct task *task;

	if (hdl == NULL)
		return;

	for (i = 0; i < OS_NR_TASKS; i++) {
		task = os_task_table[i];
		if (!task)
			continue;

		hdl(task);
	}
}

/*
 * for preempt_disable and preempt_enable need
 * to set the current task at boot stage
 */
static int __init_text task_early_init(void)
{
	struct task *task;
	struct task_info *ti;
	int i = smp_processor_id();
	extern struct task *__current_tasks[NR_CPUS];
	extern struct task *__next_tasks[NR_CPUS];
	unsigned long stack_base;

	task = &idle_tasks[i];
	memset(task, 0, sizeof(struct task));
	get_per_cpu(idle_task, i) = task;
	stack_base = CONFIG_MINOS_ENTRY_ADDRESS - i * TASK_STACK_SIZE;

	__current_tasks[i] = task;
	__next_tasks[i] = task;

	/* init the task info for the thread */
	ti = (struct task_info *)(stack_base -
			sizeof(struct task_info));

	TASK_INFO_INIT(ti, task, i);

	return 0;
}
early_initcall_percpu(task_early_init);

int create_percpu_tasks(char *name, task_func_t func, void *arg,
		uint8_t prio, size_t ss, unsigned long flags)
{
	int cpu;
	struct task *ret;

	if (prio <= OS_LOWEST_REALTIME_PRIO)
		return -EINVAL;

	for_each_online_cpu(cpu) {
		ret = create_task(name, func, arg, prio, cpu, ss, flags);
		if (ret == NULL)
			pr_err("create [%s] fail on cpu%d\n", name, cpu);
	}

	return 0;
}

struct task *create_migrating_task(char *name, task_func_t func, void *arg,
		uint8_t prio, size_t ss, unsigned long flags)
{
	if (prio <= OS_LOWEST_REALTIME_PRIO)
		return NULL;

	return create_task(name, func, arg, prio, PCPU_AFF_ANY, ss, flags);
}

struct task *create_local_task(char *name, task_func_t func, void *arg,
		uint8_t prio, size_t ss, unsigned long flags)
{
	if (prio <= OS_LOWEST_REALTIME_PRIO)
		return NULL;

	return create_task(name, func, arg, prio, PCPU_AFF_LOCAL, ss, flags);
}

struct task *create_task_on_cpu(char *name, task_func_t func, void *arg,
		uint8_t prio, int cpu, size_t ss, unsigned long flags)
{
	if (prio <= OS_LOWEST_REALTIME_PRIO)
		return NULL;

	return create_task(name, func, arg, prio, cpu, ss, flags);
}

struct task *create_realtime_task(char *name, task_func_t func, void *arg,
		uint8_t prio, size_t ss, unsigned long flags)
{
	if (prio > OS_LOWEST_REALTIME_PRIO)
		return NULL;

	return create_task(name, func, arg, prio, 0, ss, flags);
}

struct task *create_vcpu_task(char *name, task_func_t func,
		void *arg, int aff, unsigned long flags)
{
	return create_task(name, func, arg, OS_PRIO_VCPU,
			aff, 0, flags | TASK_FLAGS_VCPU);
}
