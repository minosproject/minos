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
DEFINE_PER_CPU(atomic_t, __need_resched);
DEFINE_PER_CPU(atomic_t, __preempt);
DEFINE_PER_CPU(atomic_t, __int_nesting);
DEFINE_PER_CPU(int, __os_running);

extern void sched_tick_disable(void);
extern void sched_tick_enable(unsigned long exp);

void pcpu_resched(int pcpu_id)
{
	send_sgi(CONFIG_MINOS_RESCHED_IRQ, pcpu_id);
}

void set_task_ready(struct task *task)
{
	unsigned long flags;
	struct pcpu *pcpu;

	if (is_idle_task(task))
		return;

	if (task->prio > OS_LOWEST_PRIO) {
		kernel_lock_irqsave(flags);

		os_rdy_grp |= task->bity;
		os_rdy_table[task->by] |= task->bitx;

		kernel_unlock_irqrestore(flags);
	} else {
		pcpu = get_per_cpu(pcpu, task->affinity);
		spin_lock_irqsave(&pcpu->lock, flags);

		if (task->list.next != NULL)
			list_del(&task->stat_list);
		list_add_tail(&pcpu->ready_list, &task->stat_list);

		spin_unlock_irqrestore(&pcpu->lock, flags);
	}
}

void set_task_suspend(struct task *task)
{

}

/*
 * this function can be only called by the
 * current task itself
 */
void task_sleep(uint32_t ms)
{
	struct task *task = get_current_task();

	task->delay = ms;
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

void sched_new(void)
{
	/*
	 * this function need always called with
	 * interrupt disabled
	 */
	uint8_t x, y, p;

#ifndef CONFIG_OS_REALTIME_CORE0
	int i, j = 0, k = 0;
	prio_t rdy_grp;
	prio_t ncpu_highest[NR_CPUS];
	uint64_t __rdy_table = __os_rdy_table;
	uint8_t *rdy_table;

	/*
	 * first check the rt task in the global task
	 * table, if there is no any realtime task ready
	 * just exist
	 */
	memset(ncpu_highest, OS_PRIO_IDLE, sizeof(ncpu_highest));
	if (__rdy_table == 0)
		return;

	rdy_grp = os_rdy_grp;
	rdy_table = (uint8_t *)&__rdy_table;

	for (i = 0; i < NR_CPUS; i++) {
		y = os_prio_map_table[rdy_grp];
		x = os_prio_map_table[rdy_table[y]];
		p = (y << 3) + x;
		ncpu_highest[i] = p;
		k++;

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
			for (j = 0; j < NR_CPUS; j++) {
				if (sched_array_contain(ncpu_highest[j], os_prio_cur))
					continue;

				os_highest_rdy[i] = os_prio_cur[j];
			}
		}
	}

	for (i = 0; i < NR_CPUS; i++) {
		if (os_highest_rdy[i] == OS_PRIO_IDLE)
			os_highest_rdy[i] = OS_PRIO_PCPU;
	}
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

static void save_task_context(struct task *task)
{

}

static void restore_task_state(struct task *task)
{

}

static void __switch_to_task(struct pcpu *pcpu,
		struct task *cur, struct task *next)
{
	save_task_context(cur);
	restore_task_state(next);

	/*
	 * put the next run task to the tail of the
	 * ready list
	 */
	if (cur->prio == OS_PRIO_PCPU) {
		spin_lock(&pcpu->lock);
		list_del(&next->stat_list);
		list_add_tail(&pcpu->ready_list, &next->stat_list);
		spin_unlock(&pcpu->lock);
	}

	do_hooks((void *)next, NULL, OS_HOOK_TASK_SWITCH_TO);
}

unsigned long sched_tick_handler(unsigned long data)
{
	return 0;
}

void sched(void)
{
	int i;
	struct pcpu *pcpu;
	unsigned long flags;
	int sched_flag = 0;
	struct task *cur, *next;
	int cpuid = smp_processor_id();

	if (unlikely(int_nesting()))
		panic("os_sched can not be called in interrupt\n");

	/* if (sched_lock_nesting())
		return; */

	kernel_lock_irqsave(flags);
	sched_new();
	for (i = 0; i < NR_CPUS; i++) {
		if (os_prio_cur[i] != os_highest_rdy[i]) {
			if (i == cpuid) {
				pcpu = get_per_cpu(pcpu, cpuid);
				cur = get_current_task();
				next = get_next_run_task(pcpu);
				sched_flag = 1;
			} else
				pcpu_resched(i);
		}
	}
	kernel_unlock_irqrestore(flags);

	if (sched_flag) {
		local_irq_save(flags);
		__switch_to_task(pcpu, cur, next);
		dsb();
		// arch_switch_task_sw();
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
	irq_softirq_exit();

	/*
	 * if preempt is not allowed and irq is taken from
	 * guest, then will sched()
	 */
	if (need_resched() && preempt_allowed()) {
		sched_new();
	}
}

int sched_can_idle(struct pcpu *pcpu)
{
	return 0;
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
	return 0;
}

int local_sched_init(void)
{
	struct pcpu *pcpu = get_cpu_var(pcpu);

	pcpu->state = PCPU_STATE_RUNNING;

	return request_irq(CONFIG_MINOS_RESCHED_IRQ, resched_handler,
			0, "resched handler", NULL);
}
