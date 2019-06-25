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
#include <minos/sched.h>
#include <minos/irq.h>
#include <minos/softirq.h>
#include <minos/vmodule.h>

static uint8_t const os_prio_map_table[256] = {
	0u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x00 to 0x0F */
	4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x10 to 0x1F */
	5u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x20 to 0x2F */
	4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x30 to 0x3F */
	6u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x40 to 0x4F */
	4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x50 to 0x5F */
	5u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x60 to 0x6F */
	4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x70 to 0x7F */
	7u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x80 to 0x8F */
	4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x90 to 0x9F */
	5u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0xA0 to 0xAF */
	4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0xB0 to 0xBF */
	6u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0xC0 to 0xCF */
	4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0xD0 to 0xDF */
	5u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0xE0 to 0xEF */
	4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u  /* 0xF0 to 0xFF */
};

static prio_t os_rdy_grp;
static uint64_t __os_rdy_table;
static uint8_t *os_rdy_table;
prio_t os_highest_rdy[NR_CPUS];
prio_t os_prio_cur[NR_CPUS];

extern struct task *os_task_table[OS_NR_TASKS];

static struct pcpu pcpus[NR_CPUS];

DEFINE_PER_CPU(struct pcpu *, pcpu);
DEFINE_PER_CPU(struct task *, percpu_current_task);
DEFINE_PER_CPU(struct task *, percpu_next_task);
DEFINE_PER_CPU(int, __need_resched);
DEFINE_PER_CPU(int, __preempt);
DEFINE_PER_CPU(int, __int_nesting);
DEFINE_PER_CPU(int, __os_running);

extern void sched_tick_disable(void);
extern void sched_tick_enable(unsigned long exp);

void pcpu_resched(int pcpu_id)
{
	send_sgi(CONFIG_MINOS_RESCHED_IRQ, pcpu_id);
}

void pcpu_need_resched(void)
{
	/*
	 * if the current prio PCPU, now the current task
	 * is going to sleep or another task which has been
	 * waked up by a signal, then need to resched TBD
	 */
	if (os_prio_cur[smp_processor_id()] == OS_PRIO_PCPU)
		set_need_resched();
}

void set_task_ready(struct task *task)
{
	struct pcpu *pcpu;

	/*
	 * when call this function need to ensure :
	 * 1 - kernel sched lock is locked
	 * 2 - the interrupt is disabled
	 */
	if (is_idle_task(task))
		return;

	if (is_realtime_task(task)) {
		os_rdy_grp |= task->bity;
		os_rdy_table[task->by] |= task->bitx;
	} else {
		pcpu = get_cpu_var(pcpu);
		if (pcpu->pcpu_id != task->affinity)
			panic("can not ready task by other cpu\n");

		list_del(&task->stat_list);
		list_add(&pcpu->ready_list, &task->stat_list);
	}
}

void set_task_sleep(struct task *task)
{
	struct pcpu *pcpu;

	if (is_idle_task(task))
		return;

	if (is_realtime_task(task)) {
		os_rdy_grp &= ~task->bity;
		os_rdy_table[task->by] &= ~task->bitx;
	} else {
		pcpu = get_cpu_var(pcpu);
		if (pcpu->pcpu_id != task->affinity)
			panic("can not sleep task by other cpu\n");

		list_del(&task->stat_list);
		list_add(&pcpu->sleep_list, &task->stat_list);
	}
}

void set_task_suspend(uint32_t delay)
{
	struct task *task = get_current_task();

	task_lock(task);
	task->delay = delay;
	task->stat |= TASK_STAT_SUSPEND;
	task_unlock(task);

	sched();
}

struct task *get_highest_task(uint8_t group, prio_t *ready)
{
	uint8_t x, y;

	y = os_prio_map_table[group];
	x = os_prio_map_table[ready[y]];

	return os_task_table[(y << 3) + x];
}

static int inline sched_array_contain(prio_t prio, prio_t *array)
{
	int i;

	for (i = 0; i < NR_CPUS; i++) {
		if (prio == array[i])
			return 1;
	}

	return 0;
}

static void sched_new(struct pcpu *pcpu)
{
	/*
	 * this function need always called with
	 * interrupt disabled
	 */
	uint8_t x, y, p;

#ifndef CONFIG_OS_REALTIME_CORE0
	int i, j = 0;
	prio_t rdy_grp;
	prio_t ncpu_highest[NR_CPUS];
	uint64_t __rdy_table = __os_rdy_table;
	uint8_t *rdy_table;

	/*
	 * first check the rt task in the global task
	 * table, if there is no any realtime task ready
	 * just exist
	 */
	if (__rdy_table == 0) {
		memset(os_highest_rdy, OS_PRIO_PCPU, sizeof(os_highest_rdy));
		return;
	}

	rdy_grp = os_rdy_grp;
	rdy_table = (uint8_t *)&__rdy_table;
	memset(ncpu_highest, OS_PRIO_IDLE + 1, sizeof(ncpu_highest));

	for (i = 0; i < NR_CPUS; i++) {
		y = os_prio_map_table[rdy_grp];
		x = os_prio_map_table[rdy_table[y]];
		p = (y << 3) + x;
		ncpu_highest[i] = p;

		/* clear the task ready bit */
		if ((rdy_table[y] &= ~(1 << x)) == 0)
			rdy_grp &= ~(1 << y);

		if (__rdy_table == 0)
			break;
	}

	for (i = 0; i < NR_CPUS; i++) {
		if (sched_array_contain(os_prio_cur[i], ncpu_highest))
			os_highest_rdy[i] = os_prio_cur[i];
		else {
			for (j = i; j < NR_CPUS; j++) {
				if (sched_array_contain(ncpu_highest[j],
							os_prio_cur))
					continue;

				os_highest_rdy[i] = ncpu_highest[j];
				break;
			}
		}
	}

	for (i = 0; i < NR_CPUS; i++) {
		if (os_highest_rdy[i] > OS_PRIO_PCPU)
			os_highest_rdy[i] = OS_PRIO_PCPU;
	}

	dsb();
#else
	/*
	 * only update the highest realtime task to
	 * the core0
	 */
	if (__rdy_table == 0)
		return;

	y = os_prio_map_table[rdy_grp];
	x = os_prio_map_table[rdy_table[y]];
	p = (y << 3) + x;
	os_highest_rdy[0] = p;
	dsb();
#endif
}

static struct task *get_next_run_task(struct pcpu *pcpu)
{
	prio_t prio = os_highest_rdy[pcpu->pcpu_id];

	if (prio <= OS_LOWEST_PRIO)
		return os_task_table[prio];

	if (!is_list_empty(&pcpu->ready_list))
		return list_first_entry(&pcpu->ready_list,
				struct task, stat_list);

	return pcpu->idle_task;
}

static inline void save_task_context(struct task *task)
{
	save_task_vmodule_state(task);
}

static inline void restore_task_context(struct task *task)
{
	restore_task_vmodule_state(task);
}

void switch_to_task(struct task *cur, struct task *next)
{
	int cpuid = smp_processor_id();
	// unsigned long flags;

	// need to acquire the kernel lock ?
	// kernel_lock_irqsave(flags);
	if (cur == next)
		return;

	save_task_context(cur);
	restore_task_context(next);

	/*
	 * check the current task's stat and do some action
	 * to it, first if it is a percpu task, then put it
	 * to the sleep list of its affinity pcpu, then
	 * check whether it suspend time is set or not
	 */
	if (!is_task_ready(cur)) {
		if (cur->delay) {
			mod_timer(&cur->delay_timer,
				NOW() + MILLISECS(cur->delay));
		}
	}

	/*
	 * if the next running task prio is OS_PRIO_PCPU, it
	 * need to enable the sched timer for fifo task sched
	 * otherwise disable it.
	 */
	if (is_percpu_task(next)) {
		sched_tick_enable(MILLISECS(next->run_time));
		next->start_ns = NOW();
	} else
		sched_tick_disable();

	do_hooks((void *)next, NULL, OS_HOOK_TASK_SWITCH_TO);

	/* set the current prio to the highest ready */
	os_prio_cur[cpuid] = os_highest_rdy[cpuid];

	// kernel_unlock_irqrestore(flags);
}

unsigned long sched_tick_handler(unsigned long data)
{
	struct pcpu *pcpu = get_cpu_var(pcpu);
	struct task *next, *task;

	task = get_current_task();

	if (task->prio != OS_PRIO_PCPU)
		panic("wrong task type on tick handler %d\n", task->pid);

	if ((NOW() - task->start_ns) < (task->run_time * 1000000))
		pr_warn("Bug happend on timer tick sched\n");

	list_del(&task->stat_list);
	list_add_tail(&pcpu->ready_list, &task->stat_list);
	task->run_time = CONFIG_TASK_RUN_TIME;

	/*
	 * select the next running task from the pcpu list
	 * if there is one task, need to reenalbe the sched
	 * timer
	 */
	next = list_first_entry(&pcpu->ready_list, struct task, stat_list);
	if (task != next)
		set_next_task(next);
	else
		sched_tick_enable(MILLISECS(next->run_time));

	clear_need_resched();

	return 0;
}

static inline void recal_task_time_ready(struct task *task, struct pcpu *pcpu)
{
	unsigned long now;

	if (!is_percpu_task(task))
		return;

	now = (NOW() - task->start_ns) / 1000000;
	if (now < 15) {
		task->run_time = 85 + now;

		list_del(&task->stat_list);
		list_add_tail(&pcpu->ready_list, &task->stat_list);
	}
}

void sched(void)
{
	int i;
	struct pcpu *pcpu;
	unsigned long flags;
	int sched_flag = 0;
	struct task *cur = get_current_task();
	struct task *next = cur;
	int cpuid = smp_processor_id();

	if (unlikely(int_nesting()))
		panic("os_sched can not be called in interrupt\n");

	if (!preempt_allowed() || atomic_read(&cur->lock_cpu))
		return;

	kernel_lock_irqsave(flags);

	/*
	 * need to check whether the current task is to
	 * pending on some thing or suspend, then call
	 * sched_new to get the next run task
	 */
	pcpu = get_per_cpu(pcpu, cpuid);
	if (!is_task_ready(cur)) {
		sched_flag = 1;
		set_task_sleep(cur);
	}

	sched_new(pcpu);

	/* check whether current pcpu need to resched */
	for (i = 0; i < NR_CPUS; i++) {
		if ((os_prio_cur[i] != os_highest_rdy[i])) {
			if (i == cpuid)
				sched_flag = 1;
			else
				pcpu_resched(i);
		}
	}

	/*
	 * if this task is a percpu task and it will sched
	 * out not because its run time is expries, then will
	 * set it to the correct stat
	 */
	if (sched_flag || is_idle_task(cur)) {
		if (is_task_ready(cur))
			recal_task_time_ready(cur, pcpu);

		next = get_next_run_task(pcpu);
	}

	kernel_unlock_irqrestore(flags);

	if (cur != next) {
		local_irq_save(flags);
		switch_to_task(cur, next);
		set_next_task(next);
		dsb();
		arch_switch_task_sw();
		local_irq_restore(flags);
	}
}

void irq_enter(gp_regs *regs)
{
	do_hooks(get_current_task(), (void *)regs,
			MINOS_HOOK_TYPE_ENTER_IRQ);
}

void irq_exit(gp_regs *regs)
{
	int i, sched_flag;
	struct task *task = get_current_task();
	struct task *next = task;
	int cpuid = smp_processor_id();
	struct pcpu *pcpu = get_per_cpu(pcpu, cpuid);

	irq_softirq_exit();

	/*
	 * if preempt is disabled or current task lock
	 * the cpu just return
	 */
	if (!preempt_allowed() || !need_resched() ||
			atomic_read(&task->lock_cpu))
		return;

	/*
	 * if preempt is not allowed and irq is taken from
	 * guest, then will sched()
	 */
	kernel_lock();

	sched_new(pcpu);
	for (i = 0; i < NR_CPUS; i++) {
		if (os_prio_cur[i] != os_highest_rdy[i]) {
			if (i == cpuid)
				sched_flag = 1;
			else
				pcpu_resched(i);
		}
	}

	/*
	 * if need sched or the current task is idle, then
	 * try to get the next task to check whether need
	 * to sched to anther task
	 */
	if (sched_flag || is_idle_task(task)) {
		recal_task_time_ready(task, pcpu);
		next = get_next_run_task(pcpu);
		if (next != task)
			set_next_task(next);
	}

	kernel_unlock();
	dsb();
}

int sched_can_idle(struct pcpu *pcpu)
{
	return 1;
}

void pcpus_init(void)
{
	int i;
	struct pcpu *pcpu;

	for (i = 0; i < NR_CPUS; i++) {
		pcpu = &pcpus[i];
		pcpu->state = PCPU_STATE_OFFLINE;
		init_list(&pcpu->task_list);
		init_list(&pcpu->ready_list);
		init_list(&pcpu->sleep_list);
		init_list(&pcpu->new_list);
		pcpu->pcpu_id = i;
		get_per_cpu(pcpu, i) = pcpu;
		spin_lock_init(&pcpu->lock);
	}
}

int sched_init(void)
{
	os_rdy_table = (uint8_t *)&__os_rdy_table;

	return 0;
}

int resched_handler(uint32_t irq, void *data)
{
	int sched = 0, cpuid;
	struct task *task, *n;
	struct pcpu *pcpu = get_cpu_var(pcpu);

	/*
	 * check whether there new task need to add to
	 * this pcpu
	 */
	spin_lock(&pcpu->lock);
	list_for_each_entry_safe(task, n, &pcpu->new_list, stat_list) {
		list_del(&task->stat_list);
		if (task->stat == TASK_STAT_RDY)
			list_add_tail(&pcpu->ready_list, &task->stat_list);
		else if (task->stat == TASK_STAT_SUSPEND)
			list_add_tail(&pcpu->sleep_list, &task->stat_list);
		else
			pr_err("wrong task state when create this task\n");
	}
	spin_unlock(&pcpu->lock);

	/*
	 * scan all the sleep list to see which percpu task
	 * need to change the stat
	 */
	list_for_each_entry_safe(task, n, &pcpu->sleep_list, stat_list) {
		if (task->stat == TASK_STAT_RDY) {
			list_del(&task->stat_list);
			list_add_tail(&pcpu->ready_list, &task->stat_list);
			if (task->delay) {
				task->delay = 0;
				del_timer(&task->delay_timer);
			}
		}
	}

	kernel_lock();

	/*
	 * if the current task need to sleep, then set
	 * the correct right
	 */
	cpuid = pcpu->pcpu_id;
	n = task = get_current_task();

	if ((!is_task_ready(task))) {
		sched = 1;
		set_task_sleep(task);
	}

	/* prio changed need to select a new task to sched */
	if ((os_prio_cur[cpuid] != os_highest_rdy[cpuid]) || sched) {
		if (is_task_ready(task))
			recal_task_time_ready(task, pcpu);
		n = get_next_run_task(pcpu);
	}

	if (n != task)
		set_next_task(n);
	else
		os_prio_cur[cpuid] = os_highest_rdy[cpuid];

	clear_need_resched();
	kernel_unlock();

	return 0;
}

int local_sched_init(void)
{
	struct pcpu *pcpu = get_cpu_var(pcpu);

	pcpu->state = PCPU_STATE_RUNNING;

	return request_irq(CONFIG_MINOS_RESCHED_IRQ, resched_handler,
			0, "resched handler", NULL);
}
