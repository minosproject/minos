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
#include <minos/of.h>
#include <minos/bootarg.h>
#include <minos/mm.h>

#ifdef CONFIG_VIRT
#include <virt/virt.h>
#include <virt/vm.h>
#endif

DEFINE_PER_CPU(struct pcpu *, pcpu);

extern struct task *os_task_table[OS_NR_TASKS];

#define sched_check()								\
	do {									\
		if (irq_disabled() && !preempt_allowed())			\
			panic("sched is disabled %s %d\n", __func__, __LINE__);	\
	} while (0)

static inline void add_task_to_ready_list(struct pcpu *pcpu,
		struct task *task, int preempt)
{
	ASSERT(task->stat_list.next == NULL);
	if (preempt)
		list_add(&pcpu->ready_list[task->prio], &task->stat_list);
	else
		list_add_tail(&pcpu->ready_list[task->prio], &task->stat_list);
	pcpu->tasks_in_prio[task->prio]++;
	pcpu->local_rdy_grp |= BIT(task->prio);
	wmb();

	if (preempt || current->prio > task->prio)
		set_need_resched();
}

static inline void remove_task_from_ready_list(struct pcpu *pcpu, struct task *task)
{
	ASSERT(task->stat_list.next != NULL);
	list_del(&task->stat_list);
	pcpu->tasks_in_prio[task->prio]--;
	if (is_list_empty(&pcpu->ready_list[task->prio]))
		pcpu->local_rdy_grp &= ~BIT(task->prio);
	wmb();
}

void pcpu_resched(int pcpu_id)
{
	send_sgi(CONFIG_MINOS_RESCHED_IRQ, pcpu_id);
}

void pcpu_irqwork(int pcpu_id)
{
	send_sgi(CONFIG_MINOS_IRQWORK_IRQ, pcpu_id);
}

static int select_task_run_cpu(void)
{
	/*
	 * TBD
	 */
	return (NR_CPUS - 1);
}

static void percpu_task_ready(struct pcpu *pcpu, struct task *task, int preempt)
{
	unsigned long flags;

	local_irq_save(flags);
	add_task_to_ready_list(pcpu, task, preempt);
	local_irq_restore(flags);
}

static inline void smp_percpu_task_ready(struct pcpu *pcpu,
		struct task *task, int preempt)
{
	unsigned long flags;

	if (preempt)
		task_set_resched(task);

	ASSERT(task->stat_list.next == NULL);
	spin_lock_irqsave(&pcpu->lock, flags);
	list_add_tail(&pcpu->new_list, &task->stat_list);
	spin_unlock_irqrestore(&pcpu->lock, flags);

	pcpu_irqwork(pcpu->pcpu_id);
}

int task_ready(struct task *task, int preempt)
{
	struct pcpu *pcpu, *tpcpu;

	preempt_disable();

	task->cpu = task->affinity;
	if (task->cpu == -1)
		task->cpu = select_task_run_cpu();

	/*
	 * if the task is a precpu task and the cpu is not
	 * the cpu which this task affinity to then put this
	 * cpu to the new_list of the pcpu and send a resched
	 * interrupt to the pcpu
	 */
	pcpu = get_pcpu();
	if (pcpu->pcpu_id != task->cpu) {
		tpcpu = get_per_cpu(pcpu, task->cpu);
		smp_percpu_task_ready(tpcpu, task, preempt);
	} else {
		percpu_task_ready(pcpu, task, preempt);
	}

	preempt_enable();

	return 0;
}

void task_sleep(uint32_t delay)
{
	struct task *task = current;

	/*
	 * task sleep will wait for the sleep timer expired
	 * or the event happend
	 */
	do_not_preempt();
	task->delay = delay;
	task->stat = TASK_STAT_WAIT_EVENT;
	task->wait_type = TASK_EVENT_TIMER;

	sched();
}

static struct task *pick_next_task(struct pcpu *pcpu)
{
	struct list_head *head;
	struct task *task = current;
	int prio;

	/*
	 * if the current task need to sleep or waitting some
	 * event happen. delete it from the ready list, then the
	 * next run task can be got.
	 */
	if (!task_is_running(task)) {
		remove_task_from_ready_list(pcpu, task);
                if (task->stat == TASK_STAT_STOP)
                        list_add_tail(&pcpu->stop_list, &task->stat_list);
	}

	/*
	 * get the highest ready task list to running
	 */
	prio = ffs_one_table[pcpu->local_rdy_grp];
	ASSERT(prio != -1);
	head = &pcpu->ready_list[prio];

	/*
	 * get the first task, then put the next running
	 * task to the end of the ready list.
	 */
	ASSERT(!is_list_empty(head));
	task = list_first_entry(head, struct task, stat_list);
	list_del(&task->stat_list);
	list_add_tail(head, &task->stat_list);

	return task;
}

void switch_to_task(struct task *cur, struct task *next)
{
	struct pcpu *pcpu = get_pcpu();
	unsigned long now;

	arch_task_sched_out(cur);
	do_hooks((void *)cur, NULL, OS_HOOK_TASK_SWITCH_OUT);

	now = NOW();

	/* 
	 * check the current task's stat and do some action
	 * to it, check whether it suspend time is set or not
	 *
	 * if the task is ready state, adjust the run time of
	 * this task. If the task need to wait some event, and
	 * need request a timeout timer then need setup the timer.
	 */
	if ((cur->stat == TASK_STAT_WAIT_EVENT) && (cur->delay > 0))
		mod_timer(&cur->delay_timer, now + MILLISECS(cur->delay));
	cur->last_cpu = cur->cpu;
	cur->run_time = CONFIG_TASK_RUN_TIME;
	smp_wmb();

	/*
	 * notify the cpu which need to waku-up this task that
	 * the task has been do to sched out, can be wakeed up
	 * safe, the task is offline now.
	 */
	cur->cpu = -1;

	/*
	 * change the current task to next task.
	 */
	next->stat = TASK_STAT_RUNNING;
	set_current_task(next);
	pcpu->running_task = next;

	arch_task_sched_in(next);
	do_hooks((void *)next, NULL, OS_HOOK_TASK_SWITCH_TO);

	next->ctx_sw_cnt++;
	next->wait_event = 0;
	next->start_ns = now;

	/*
	 * if the task count of this PRIO in this pcpu bigger than
	 * 1, need enable the sched timer.
	 */
	if (pcpu->tasks_in_prio[next->prio] > 1)
		mod_timer(&pcpu->sched_timer, now + next->run_time);

	smp_wmb();
}

static void sched_tick_handler(unsigned long data)
{
	struct task *task = current;

	/*
	 * mark this task has used its running ticket, and the sched
	 * timer is off.
	 */
	ASSERT(task_is_running(task));
	set_bit(TIF_TICK_EXHAUST, &task->ti.flags);
	set_need_resched();
}

static void do_sched(void)
{
	unsigned long flags;
	struct task *cur = current, *next;
	struct pcpu *pcpu = get_pcpu();

	local_irq_save(flags);

	/*
	 * clear the bit of TIF_NEED_RESCHED and
	 * TIF_DONOT_PREEMPT here.
	 */
	clear_bit(TIF_DONOT_PREEMPT, &cur->ti.flags);
	clear_bit(TIF_NEED_RESCHED, &cur->ti.flags);

	next = pick_next_task(pcpu);
	if (next == cur) {
		BUG_ON(cur->stat == TASK_STAT_WAIT_EVENT,
			"sched: task need sleep\n");
		goto out;
	}

	arch_switch_task_sw(cur, next);
out:
	local_irq_restore(flags);
}

void sched(void)
{
	sched_check();

	/*
	 * the task has been already puted in to sleep stat
	 * and it is not on the ready list or it prio bit is
	 * cleared, so this task's context can only accessed by
	 * the current running cpu.
	 *
	 * so if the task is running state and want to drop the cpu
	 * need call sched_yield not sched
	 */
	do {
		preempt_disable();
		do_sched();
		preempt_enable();
	} while (need_resched());
}

/*
 * notify all the cpu to do a resched if need
 */
void cpus_resched(void)
{
	int cpuid, cpu;
	unsigned long flags;

	local_irq_save(flags);
	cpuid = smp_processor_id();
	for_each_online_cpu(cpu) {
		if (cpu != cpuid)
			pcpu_resched(cpu);
	}
	local_irq_restore(flags);
}

void irq_enter(gp_regs *regs)
{
	current_task_info->flags |= __TIF_HARDIRQ_MASK;
	wmb();
}

void irq_exit(gp_regs *regs)
{
	current_task_info->flags &= ~__TIF_HARDIRQ_MASK;
	wmb();
}

static void task_run_again(struct pcpu *pcpu, struct task *task)
{
	unsigned long now = NOW();

	task->ti.flags &= ~__TIF_TICK_EXHAUST;
	task->start_ns = now;
}

void task_exit(int errno)
{
	set_current_stat(TASK_STAT_STOP);
	sched();
}

void exception_return_handler(void)
{
	struct task *next, *task = current;
	struct task_info *ti = to_task_info(task);
	struct pcpu *pcpu = get_pcpu();

	/*
	 * if the task is suspend state, means next the cpu
	 * will call sched directly, so do not sched out here
	 */
	if ((ti->preempt_count > 0) || (ti->flags & __TIF_DONOT_PREEMPT)) {
		if (ti->flags & __TIF_TICK_EXHAUST)
			task_run_again(pcpu, task);
		return;
	}

	next = pick_next_task(pcpu);
	if ((next == task)) {
		if (ti->flags & __TIF_TICK_EXHAUST)
			task_run_again(pcpu, task);
		return;
	}

	ti->flags &= ~(__TIF_TICK_EXHAUST | __TIF_NEED_RESCHED);
	switch_to_task(task, next);
}

int sched_init(void)
{
	return 0;
}

static int irqwork_handler(uint32_t irq, void *data)
{
	struct pcpu *pcpu = get_pcpu();
	struct task *task, *n;
	int preempt = 0;

	/*
	 * check whether there are new taskes need to
	 * set to ready state again
	 */
	raw_spin_lock(&pcpu->lock);
	list_for_each_entry_safe(task, n, &pcpu->new_list, stat_list) {
		/*
		 * remove it from the new_next.
		 */
		list_del(&task->stat_list);

		if (task->stat == TASK_STAT_RUNNING) {
			pr_err("task %s state %d wrong\n",
				task->name? task->name : "Null", task->stat);
			continue;
		}

		preempt += task_need_resched(task);
		task_clear_resched(task);

		add_task_to_ready_list(pcpu, task, 0);

		/*
		 * if the task has delay timer, cancel it.
		 */
		if (task->delay) {
			del_timer(&task->delay_timer);
			task->delay = 0;
		}
	}
	raw_spin_unlock(&pcpu->lock);

	if (preempt || task_is_idle(current))
		set_need_resched();

	return 0;
}

static int resched_handler(uint32_t irq, void *data)
{
	set_need_resched();
	return 0;
}

int local_sched_init(void)
{
	struct pcpu *pcpu = get_pcpu();

	init_list(&pcpu->new_list);
	init_list(&pcpu->stop_list);
	init_list(&pcpu->ready_list[0]);
	init_list(&pcpu->ready_list[1]);
	init_list(&pcpu->ready_list[2]);
	init_list(&pcpu->ready_list[3]);
	init_list(&pcpu->ready_list[4]);
	init_list(&pcpu->ready_list[5]);
	init_list(&pcpu->ready_list[6]);
	init_list(&pcpu->ready_list[7]);

	init_timer(&pcpu->sched_timer);
	pcpu->sched_timer.cpu = pcpu->pcpu_id;
	pcpu->sched_timer.function = sched_tick_handler;
	pcpu->sched_timer.data = (unsigned long)pcpu;

	pcpu->state = PCPU_STATE_RUNNING;

	request_irq(CONFIG_MINOS_RESCHED_IRQ, resched_handler,
			0, "resched handler", NULL);
	request_irq(CONFIG_MINOS_IRQWORK_IRQ, irqwork_handler,
			0, "irqwork handler", NULL);
	return 0;
}

int __wake_up(struct task *task, long pend_state, void *data)
{
	unsigned long flags;
	uint32_t timeout;

	if (task == current)
		return 0;

	preempt_disable();

	spin_lock_irqsave(&task->s_lock, flags);

	/*
	 * task already waked up, if the stat is set to
	 * TASK_STAT_WAIT_EVENT, it means that the task will
	 * call sched() to sleep or wait something happen.
	 */
	if (task->stat != TASK_STAT_WAIT_EVENT) {
		spin_unlock_irqrestore(&task->s_lock, flags);
		preempt_enable();
		return 0;
	}

	/*
	 * the task may in sched() routine on other cpu
	 * wait the task really out of running. since the task
	 * will not preempt in the kernel space now, so the cpu
	 * of the task will change to -1 at one time.
	 *
	 * since the kernel can not be preempted so it can make
	 * sure that sched() can be finish its work.
	 */
	while (task->cpu != -1)
		cpu_relax();

	/*
	 * here this cpu got this task, and can set the new
	 * state to running and run it again.
	 */
	task->pend_stat = pend_state;
	task->stat = TASK_STAT_WAKING;
	task->msg = data;
	timeout = task->delay;
	task->delay = 0;

	spin_unlock_irqrestore(&task->s_lock, flags);

	/*
	 * here it means that this task has not been timeout, so can
	 * delete the timer for this task.
	 */
	if (timeout && (task->pend_stat != TASK_STAT_PEND_TO))
		del_timer_sync(&task->delay_timer);

	/*
	 * find a best cpu to run this task.
	 */
	task_ready(task, 0);
	preempt_enable();

	return 0;
}
