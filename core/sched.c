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

#ifdef CONFIG_VIRT
#include <virt/virt.h>
#include <virt/vm.h>
#endif

DEFINE_SPIN_LOCK(__kernel_lock);

static uint8_t __align(8) os_rdy_grp;
static uint64_t __os_rdy_table;
static uint8_t *os_rdy_table = (uint8_t *)&__os_rdy_table;
uint8_t os_highest_rdy[NR_CPUS];
uint8_t os_prio_cur[NR_CPUS];

static struct pcpu pcpus[NR_CPUS];

DEFINE_PER_CPU(struct pcpu *, pcpu);
DEFINE_PER_CPU(int, __os_running);

struct task *__current_tasks[NR_CPUS];
struct task *__next_tasks[NR_CPUS];

#define SCHED_CLASS_LOCAL	0
#define SCHED_CLASS_GLOBAL	1
static int pcpu_sched_class[NR_CPUS];
static int nr_global_sched_cpus;

extern struct task *os_task_table[OS_NR_TASKS];
extern void sched_tick_disable(void);
extern void sched_tick_enable(unsigned long exp);

#define sched_check()		\
	do {				\
		if (irq_disabled() && !preempt_allowed())	\
			panic("sched is disabled %s %d\n", __func__, __LINE__);	\
	} while (0)

#define sched_yield_check()	\
	do {				\
		if (in_interrupt() && !preempt_allowed())	\
			panic("sched is disabled %s %d\n", __func__, __LINE__);	\
	} while (0)

static inline void __percpu_task_ready(struct pcpu *pcpu,
		struct task *task, int preempt)
{
	if (preempt)
		add_task_to_ready_list(pcpu, task);
	else
		add_task_to_ready_list_tail(pcpu, task);

	pcpu->local_rdy_grp |= task->local_mask;
}

static inline void __percpu_task_sleep(struct pcpu *pcpu,
		struct task *task)
{
	list_del(&task->stat_list);
	task->stat_list.next = NULL;

	if (is_list_empty(&pcpu->ready_list[task->local_prio]))
		pcpu->local_rdy_grp &= ~task->local_mask;
}

static inline void percpu_task_ready(struct pcpu *pcpu,
		struct task *task, int preempt)
{
	if (task->stat_list.next != NULL) {
		pr_err("%s: wrong task state %d\n", __func__, task->stat);
		list_del(&task->stat_list);
	}

	__percpu_task_ready(pcpu, task, preempt);
}

static inline void percpu_task_sleep(struct pcpu *pcpu, struct task *task)
{
	if (task->stat_list.next == NULL)
		pr_err("%s: wrong task state %d\n", __func__, task->stat);
	else
		__percpu_task_sleep(pcpu, task);
}

static inline void smp_percpu_task_ready(struct pcpu *pcpu,
		struct task *task, int preempt)
{
	unsigned long flags;

	if (preempt)
		task_set_resched(task);

	/*
	 * add this task to the pcpu's new_list and send
	 * a resched call to the pcpu
	 */
	spin_lock_irqsave(&pcpu->lock, flags);
	if (task->stat_list.next != NULL) {
		pr_err("%s: wrong task state %d\n", __func__, task->stat);
		list_del(&task->stat_list);
	}

	list_add_tail(&pcpu->new_list, &task->stat_list);
	pcpu_irqwork(task->affinity);

	spin_unlock_irqrestore(&pcpu->lock, flags);
}

static void inline set_next_task(struct task *task, int cpuid)
{
	__next_tasks[cpuid] = task;
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

int set_task_ready(struct task *task, int preempt)
{
	struct pcpu *pcpu, *tpcpu;

	if (task->stat == TASK_STAT_STOPPED)
		panic("task is stopped can not be wakeup\n");

	/*
	 * when call this function need to ensure :
	 * 1 - kernel sched lock is locked (rt task)
	 * 2 - the interrupt is disabled
	 *
	 * if the task is a precpu task and the cpu is not
	 * the cpu which this task affinity to then put this
	 * cpu to the new_list of the pcpu and send a resched
	 * interrupt to the pcpu
	 */
	if (task_is_realtime(task)) {
		os_rdy_grp |= task->bity;
		os_rdy_table[task->by] |= task->bitx;
	} else {
		pcpu = get_cpu_var(pcpu);
		if (pcpu->pcpu_id != task->affinity) {
			tpcpu = get_per_cpu(pcpu, task->affinity);
			smp_percpu_task_ready(tpcpu, task, preempt);

			return 0;
		}

		percpu_task_ready(pcpu, task, preempt);
	}

	if (task->delay) {
		del_timer(&task->delay_timer);
		task->delay = 0;
	}

	set_need_resched();

	return 0;
}

int set_task_sleep(struct task *task, uint32_t ms)
{
	struct pcpu *pcpu;

	if (task_is_realtime(task)) {
		os_rdy_table[task->by] &= ~task->bitx;
		if (os_rdy_table[task->by] == 0)
			os_rdy_grp &= ~task->bity;
	} else {
		pcpu = get_cpu_var(pcpu);

		if (task->affinity != pcpu->pcpu_id) {
			pr_warn("can not sleep other task %d\n", task->pid);
			return -EPERM;
		}

		percpu_task_sleep(pcpu, task);
	}

	task->delay = ms;
	set_need_resched();

	return 0;
}

void set_task_suspend(uint32_t delay)
{
	unsigned long flags = 0;
	struct task *task = get_current_task();

	task_lock_irqsave(task, flags);
	task->stat |= TASK_STAT_SUSPEND;
	set_task_sleep(task, delay);
	task_unlock_irqrestore(task, flags);
	
	sched();
}

struct task *get_highest_task(uint8_t group, uint8_t *ready)
{
	uint8_t x, y;
	
	y = ffs_one_table[group];
	x = ffs_one_table[ready[y]];

	return os_task_table[(y << 3) + x];
}

static void sched_new(struct pcpu *pcpu)
{
	/*
	 * this function need always called with
	 * interrupt disabled
	 */
	uint8_t x, y;

#ifndef CONFIG_OS_REALTIME_CORE0
	int i, j;
	uint8_t rdy_grp;
	uint8_t ncpu_highest[NR_CPUS];
	uint8_t current_copy[NR_CPUS];
	uint64_t __rdy_table = __os_rdy_table;
	uint8_t *rdy_table;

	/*
	 * first check the rt task in the global task
	 * table, if there is no any realtime task ready
	 * just exist
	 */
	memset(os_highest_rdy, OS_PRIO_PCPU, sizeof(os_highest_rdy));
	if (__rdy_table == 0)
		return;

	rdy_grp = os_rdy_grp;
	rdy_table = (uint8_t *)&__rdy_table;
	memcpy(current_copy, os_prio_cur, sizeof(current_copy));

	/*
	 * init the ncpu_highest to the default prio, also
	 * mask the cpu which a local sched class to 0xff
	 * these cpu will alway running percpu task
	 */
	for (i = 0; i < NR_CPUS; i++) {
		ncpu_highest[i] = OS_PRIO_PCPU;
		if (pcpu_sched_class[i] == SCHED_CLASS_LOCAL)
			current_copy[i] = OS_INVALID_PRIO;
	}

	for (i = 0; i < nr_global_sched_cpus; i++) {
		y = ffs_one_table[rdy_grp];
		x = ffs_one_table[rdy_table[y]];
		ncpu_highest[i] = (y << 3) + x;

		/* clear the task ready bit */
		if ((rdy_table[y] &= ~(1 << x)) == 0)
			rdy_grp &= ~(1 << y);

		if (__rdy_table == 0)
			break;
	}

	/*
	 * find out the task which already in the running state
	 * which do not need to change on the related cpu
	 */
	for (i = 0; i < nr_global_sched_cpus; i++) {
		for (j = 0; j < NR_CPUS; j++) {
			if (current_copy[j] == OS_INVALID_PRIO)
				continue;

			if (ncpu_highest[i] == current_copy[j]) {
				os_highest_rdy[j] = ncpu_highest[i];
				current_copy[j] = OS_INVALID_PRIO;
				ncpu_highest[i] = OS_INVALID_PRIO;
			}
		}
	}

	/*
	 * deal with the left task if it is not in the current
	 * running tasks
	 */
	for (i = 0; i < nr_global_sched_cpus; i++) {
		if (ncpu_highest[i] == OS_INVALID_PRIO)
			continue;

		for (j = 0; j < NR_CPUS; j++) {
			if (current_copy[j] == OS_INVALID_PRIO)
				continue;

			os_highest_rdy[j] = ncpu_highest[i];
			current_copy[j] = OS_INVALID_PRIO;
			ncpu_highest[i] = OS_INVALID_PRIO;

			break;
		}
	}

	wmb();
#else
	/*
	 * only update the highest realtime task to
	 * the core0
	 */
	os_highest_rdy[0] = OS_PRIO_PCPU;
	if (__os_rdy_table == 0)
		return;

	y = ffs_one_table[os_rdy_grp];
	x = ffs_one_table[os_rdy_table[y]];
	os_highest_rdy[0] = (y << 3) + x;
	wmb();
#endif
}

static void inline task_sched_return(struct task *task)
{
#ifdef CONFIG_VIRT
	gp_regs *reg;

	reg = (gp_regs *)task->stack_base;
	if (task_is_vcpu(task) && taken_from_guest(reg))
		enter_to_guest((struct vcpu *)task->pdata, NULL);
#endif
}

static void inline no_task_sched_return(struct pcpu *pcpu, struct task *task)
{
	/*
	 * if need resched but there something block to sched
	 * a new task, then need enable the sched timer, but
	 * only give the task little run time
	 */
	if ((pcpu->running_task != task)) {
		pcpu->running_task = task;
		task->start_ns = NOW();
		sched_tick_enable(MILLISECS(task->run_time));
	}

	task_sched_return(task);
}

static inline struct task *get_next_local_run_task(struct pcpu *pcpu)
{
	uint8_t prio = ffs_one_table[pcpu->local_rdy_grp];

	if (is_list_empty(&pcpu->ready_list[prio]))
		panic("no more task on ready list %d\n", prio);

	return list_first_entry(&pcpu->ready_list[prio],
			struct task, stat_list);
}

static inline struct task *get_next_global_run_task(struct pcpu *pcpu)
{
	uint8_t prio = os_highest_rdy[pcpu->pcpu_id];

	if (prio <= OS_LOWEST_REALTIME_PRIO)
		return os_task_table[prio];
	else
		return get_next_local_run_task(pcpu);
}

static inline void
recal_task_run_time(struct task *task, struct pcpu *pcpu)
{
	unsigned long now;

	if (!task_is_percpu(task))
		return;

	now = (NOW() - task->start_ns) / 1000000;
	now = now > task->run_time ? 0 : task->run_time - now;

	/*
	 * the task is preempted by other task, if the task is
	 * percpu task, here put it to the tail of the ready
	 * list ?
	 */
	if (now < 15) {
		task->run_time = CONFIG_TASK_RUN_TIME + now;
		list_del(&task->stat_list);
		add_task_to_ready_list_tail(pcpu, task);
	} else {
		task->run_time = now;
	}
}

static void global_switch_out(struct pcpu *pcpu,
		struct task *cur, struct task *next)
{
	if (task_is_percpu(next))
		os_prio_cur[pcpu->pcpu_id] = OS_PRIO_PCPU;
	else
		os_prio_cur[pcpu->pcpu_id] = next->prio;
	wmb();

	/* release the kernel lock now */
	kernel_unlock();
}

static void local_switch_out(struct pcpu *pcpu,
		struct task *cur, struct task *next)
{

}

static void global_switch_to(struct pcpu *pcpu,
		struct task *cur, struct task *next)
{

}

static void local_switch_to(struct pcpu *pcpu,
		struct task *cur, struct task *next)
{

}

static void inline task_switch_out(struct pcpu *pcpu,
		struct task *cur, struct task *next)
{
#ifdef CONFIG_VIRT
	struct vm *vm;

	if (task_is_vcpu(cur)) {
		vm = task_to_vm(cur);
		atomic_dec(&vm->vcpu_online_cnt);
	}
#endif

	pcpu->switch_out(pcpu, cur, next);
}

static void inline task_switch_to(struct pcpu *pcpu,
		struct task *cur, struct task *next)
{
#ifdef CONFIG_VIRT
	struct vm *vm;

	if (task_is_vcpu(next)) {
		vm = task_to_vm(next);
		atomic_inc(&vm->vcpu_online_cnt);
	}
#endif
	pcpu->switch_to(pcpu, cur, next);
	next->ctx_sw_cnt++;
}

void switch_to_task(struct task *cur, struct task *next)
{
	struct pcpu *pcpu = get_cpu_var(pcpu);

#ifdef CONFIG_VIRT
	if (task_is_vcpu(cur))
		save_vcpu_context(cur);
#endif

	/* 
	 * check the current task's stat and do some action
	 * to it, check whether it suspend time is set or not
	 *
	 * if the task is ready state, adjust the run time of
	 * this task
	 */
	if (!task_is_ready(cur)) {
		if (!task_is_realtime(cur))
			cur->run_time = CONFIG_TASK_RUN_TIME;
		if (cur->delay) {
			mod_timer(&cur->delay_timer,
				NOW() + MILLISECS(cur->delay));
		}
		cur->stat &= ~TASK_STAT_RUNNING;
	} else {
		recal_task_run_time(cur, pcpu);
		cur->stat = TASK_STAT_RDY;
	}

	do_hooks((void *)cur, NULL, OS_HOOK_TASK_SWITCH_OUT);
	task_switch_out(pcpu, cur, next);

	/*
	 * if the next running task prio is OS_PRIO_PCPU, it
	 * need to enable the sched timer for fifo task sched
	 * otherwise disable it.
	 */
	next->start_ns = NOW();
	pcpu->running_task = next;
	sched_tick_enable(MILLISECS(next->run_time));

#ifdef CONFIG_VIRT
	if (task_is_vcpu(next))
		restore_vcpu_context(next);
#endif

	next->stat = TASK_STAT_RUNNING;
	task_info(next)->cpu = pcpu->pcpu_id;

	do_hooks((void *)next, NULL, OS_HOOK_TASK_SWITCH_TO);
	task_switch_to(pcpu, cur, next);

	task_sched_return(next);
	wmb();
}

unsigned long sched_tick_handler(unsigned long data)
{
	struct task *task;
	struct pcpu *pcpu;
	unsigned long now;
	unsigned long delta;

	task = get_current_task();
	if (!task_is_percpu(task)) {
		pr_debug("wrong task type on tick handler %d\n", task->pid);
		return 0;
	}

	now = NOW();
	pcpu = get_cpu_var(pcpu);
	delta = now - task->start_ns;

	/*
	 * there is a case that when the sched timer has been
	 * expires when switch out task, once switch to a new
	 * task, the interrupt will be triggered, but the old
	 * task is switch out, so directly return, do not switch
	 * to other task
	 */
	if (delta < MILLISECS(task->run_time)) {
		pr_debug("Bug happend on timer tick sched 0x%p 0x%p %d %d\n",
				now, task->start_ns, task->run_time, delta);
		sched_tick_enable(MILLISECS(task->run_time - delta));

		return 0;
	}

	/*
	 * if the preempt is disable at this time, what will
	 * happend if the task is not on the head of the pcpu's
	 * ready list ? need further check.
	 */
	task->start_ns = 0;
	task->run_time = task_is_idle(task) ? 0 : CONFIG_TASK_RUN_TIME;
	pcpu->running_task = pcpu->idle_task;

	if (task_is_ready(task)) {
		list_del(&task->stat_list);
		add_task_to_ready_list_tail(pcpu, task);
	} else
		pr_warn("task is not ready now\n");

	set_need_resched();

	return 0;
}

void global_sched(struct pcpu *pcpu, struct task *cur)
{
	int i;
	struct task *next;

	kernel_lock();

	sched_new(pcpu);

	/*
	 * if there other cpu need to change the task running
	 * on the pcpu, then send the ipi to the related cpu
	 */
	for (i = 0; i < NR_CPUS; i++) {
		if ((os_prio_cur[i] != os_highest_rdy[i])) {
			if (i != pcpu->pcpu_id)
				pcpu_resched(i);
		}
	}

	next = get_next_global_run_task(pcpu);
	if (cur != next) {
		set_next_task(next, pcpu->pcpu_id);
		arch_switch_task_sw();
	} else
		kernel_unlock();
}

/*
 * local sched will only sched the task which
 * affinity to this pcpu, so in this function need
 * only disable the irq to protect the race caused
 * by tasks
 */
void local_sched(struct pcpu *pcpu, struct task *cur)
{
	struct task *next;

	/* get the next run task from the local list */
	next = get_next_local_run_task(pcpu);
	if (next == cur)
		return;

	set_next_task(next, pcpu->pcpu_id);
	arch_switch_task_sw();
}

/*
 * the task drop the cpu itself
 */
void sched_yield(void)
{
	struct pcpu *pcpu;
	struct task *cur = current;
	unsigned long flags;

	sched_yield_check();

	/*
	 * if current task is percpu task, and is not in
	 * suspend state, means it need ot drop the run time
	 * then need to set it to the tail of the ready list
	 */
	if (!task_is_realtime(cur)) {
		local_irq_save(flags);

		pcpu = get_cpu_var(pcpu);
		if (!task_is_idle(cur) && task_is_ready(cur)) {
			list_del(&cur->stat_list);
			add_task_to_ready_list_tail(pcpu, cur);
		}

		local_irq_restore(flags);
		pcpu_resched(pcpu->pcpu_id);
	} else
		pr_warn("realtime task can not call %s\n", __func__);
}

void sched(void)
{
	struct pcpu *pcpu;
	struct task *cur = get_current_task();

	sched_check();

	/*
	 * the task has been already puted in to sleep stat
	 * and it is not on the ready list or it prio bit is
	 * cleared, so this task's context can only accessed by
	 * the current running cpu.
	 *
	 * so if the is running state and want to drop the cpu
	 * need call sched_yield not sched
	 */
	while (need_resched()) {
		local_irq_disable();

		if (need_resched()) {
			clear_need_resched();
			pcpu = get_cpu_var(pcpu);
			pcpu->sched(pcpu, cur);
		}

		local_irq_enable();
	}
}

static inline void cpu_resched(void)
{
	if (!in_interrupt())
		sched_yield();
	else
		set_need_resched();
}

void sched_task(struct task *task)
{
	unsigned long flags;
	int t_cpu, s_cpu, smp_resched = 0;
	int realtime_task = task_is_realtime(task);

	/*
	 * check whether this task can be sched on this
	 * cpu, since there may some pcpu do not sched
	 * the realtime task, if the realtime task is
	 * sleep on other cpu need to send a resched signal
	 */
	local_irq_save(flags);

	s_cpu = smp_processor_id();
	t_cpu = task_is_realtime(task) ?
		task_info(task)->cpu : task->affinity;

	if (realtime_task && pcpu_sched_class[s_cpu] != SCHED_CLASS_GLOBAL)
			smp_resched = 1;

	local_irq_restore(flags);

	/*
	 * percpu task will not run on the current pcpu and the
	 * task has been set ready then send a irqwork irq to
	 * the dst pcpu, so just return
	 */
	if (!realtime_task && (s_cpu != t_cpu))
		return;

	if (smp_resched)
		pcpu_resched(t_cpu);
	else {
		if (!in_interrupt()) {
			if (task_is_realtime(current))
				sched();
			else
				sched_yield();
		} else {
			set_need_resched();
		}
	}
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
	struct task *task = get_current_task();

	current_task_info()->flags |= __TIF_HARDIRQ_MASK;

	do_hooks(task, (void *)regs, OS_HOOK_ENTER_IRQ);

#ifdef CONFIG_VIRT
	if (taken_from_guest(regs))
		exit_from_guest(task_to_vcpu(task), regs);
#endif
}

static void local_irq_handler(struct pcpu *pcpu, struct task *task)
{
	struct task *next;

	next = get_next_local_run_task(pcpu);
	if (next == task) {
		no_task_sched_return(pcpu, task);
		return;
	}

	set_next_task(next, pcpu->pcpu_id);
	switch_to_task(task, next);
}

static void global_irq_handler(struct pcpu *pcpu, struct task *task)
{
	int i;
	struct task *next = task;
	int cpuid = pcpu->pcpu_id;

	kernel_lock();

	/*
	 * get the highest prio task which need run on this
	 * cpu, and check wheter there new high priority task
	 * need to run on other cpu, if yes send a resched task
	 * to the related cpu
	 *
	 * if the os_highest rdy is not as same as the os_current
	 * means this pcup is kicked by other pcpu and has set the
	 * highest prio, do not need to call sched_new() again
	 */
	if (os_highest_rdy[cpuid] == os_prio_cur[cpuid]) {
		sched_new(pcpu);

		for (i = 0; i < NR_CPUS; i++) {
			if (os_prio_cur[i] != os_highest_rdy[i]) {
				if (i != cpuid)
					pcpu_resched(i);
			}
		}
	}

	/* 
	 * if need sched or the current task is idle, then
	 * try to get the next task to check whether need
	 * to sched to anther task
	 */
	next = get_next_global_run_task(pcpu);
	if (next != task) {
		set_next_task(next, cpuid);
		switch_to_task(task, next);
		mb();

		return;
	}

	kernel_unlock();

	no_task_sched_return(pcpu, next);
}

void irq_return_handler(struct task *task)
{
	int p, n;
	struct task_info *ti;
	struct pcpu *pcpu = get_cpu_var(pcpu);

	/*
	 * in this function, the stack is used wit irq stack
	 * not at the task context, so can not use current
	 */
	ti = task_info(task);
	p = ti->preempt_count;
	n = !(ti->flags & __TIF_NEED_RESCHED);

	/*
	 * if the task is suspend state, means next the cpu
	 * will call sched directly, so do not sched out here
	 */
	if (p || n) {
		no_task_sched_return(pcpu, task);
	} else {
		pcpu->irq_handler(pcpu, task);
		clear_bit(TIF_NEED_RESCHED, &ti->flags);
	}
}

void irq_exit(gp_regs *regs)
{
	struct task_info *tf = current_task_info();

	tf->flags &= ~__TIF_HARDIRQ_MASK;
	tf->flags |= __TIF_SOFTIRQ_MASK;
	irq_softirq_exit();
	tf->flags &= ~__TIF_SOFTIRQ_MASK;
}

int select_task_run_cpu(void)
{
	return 0;
}

static inline int
get_affinity_from_dts(struct device_node *node, uint64_t *aff)
{
	uint32_t value[2];
	int ret;

	ret = of_get_u32_array(node, "reg", value, 2);
	if (ret <= 0)
		return -EINVAL;

	if (ret == 1)
		*aff = value[0];
	else
		*aff = ((uint64_t)value[0] << 32) | value[1];

	return 0;
}

static void __used *of_setup_pcpu(struct device_node *node, void *data)
{
	int cpuid;
	uint64_t affinity;
	struct pcpu *pcpu;
	char class[16];

	if (node->class != DT_CLASS_CPU)
		return NULL;

	if (get_affinity_from_dts(node, &affinity)) {
		pr_err("get affinity from dts failed\n");
		return NULL;
	}

	cpuid = affinity_to_cpuid(affinity);
	if (cpuid >= NR_CPUS)
		return NULL;

	memset(class, 0, 16);
	if (of_get_string(node, "sched_class", class, 15) <= 0)
		return NULL;

	pr_notice("sched class of pcpu-%d: %s\n", cpuid, class);

	if (!strcmp(class, "local")) {
		pcpu = get_per_cpu(pcpu, cpuid);
		pcpu->sched = local_sched;
		pcpu->switch_out = local_switch_out;
		pcpu->switch_to = local_switch_to;
		pcpu->irq_handler = local_irq_handler;
		pcpu_sched_class[cpuid] = SCHED_CLASS_LOCAL;
	} else {
		pr_warn("unsupport sched class\n");
	}

	return node;
}

void pcpus_init(void)
{
	int i;
	struct pcpu *pcpu;

	for (i = 0; i < NR_CPUS; i++) {
		pcpu = &pcpus[i];
		pcpu->state = PCPU_STATE_OFFLINE;
		init_list(&pcpu->task_list);
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
		pcpu->pcpu_id = i;
		get_per_cpu(pcpu, i) = pcpu;
		spin_lock_init(&pcpu->lock);

		/*
		 * setup the sched function for the cpu
		 * the default is the global class
		 */
#ifdef CONFIG_OS_REALTIME_CORE0
		if (i == 0) {
			pcpu->sched = global_sched;
			pcpu->switch_out = global_switch_out;
			pcpu->switch_to = global_switch_to;
			pcpu->irq_handler = global_irq_handler;
			pcpu_sched_class[i] = SCHED_CLASS_GLOBAL;
		} else {
			pcpu->sched = local_sched;
			pcpu->switch_out = local_switch_out;
			pcpu->switch_to = local_switch_to;
			pcpu->irq_handler = local_irq_handler;
			pcpu_sched_class[i] = SCHED_CLASS_LOCAL;
		}
#else
		pcpu->sched = global_sched;
		pcpu->switch_out = global_switch_out;
		pcpu->switch_to = global_switch_to;
		pcpu->irq_handler = global_irq_handler;
		pcpu_sched_class[i] = SCHED_CLASS_GLOBAL;
#endif
	}

#ifndef CONFIG_OS_REALTIME_CORE0
#ifdef CONFIG_DEVICE_TREE
	/* get the cpu attribute from the dts */
	of_iterate_all_node_loop(hv_node, of_setup_pcpu, NULL);
#endif
#endif
	for (i = 0; i < NR_CPUS; i++) {
		if (pcpu_sched_class[i] == SCHED_CLASS_GLOBAL)
			nr_global_sched_cpus++;
	}
}

int sched_init(void)
{
	return 0;
}

static int irqwork_handler(uint32_t irq, void *data)
{
	int need_resched = 0;
	int preempt;
	struct task *task, *n;
	struct task *cur = get_current_task();
	struct pcpu *pcpu = get_cpu_var(pcpu);

	/*
	 * check whether there are new taskes need to
	 * set to ready state again
	 */
	raw_spin_lock(&pcpu->lock);
	list_for_each_entry_safe(task, n, &pcpu->new_list, stat_list) {
		if (!task_is_ready(task)) {
			pr_warn("wrong task state in cpu new list\n");
			continue;
		}

		if (task->prio < cur->prio)
			need_resched = 1;

		preempt = task_need_resched(task);
		need_resched += preempt;
		list_del(&task->stat_list);
		__percpu_task_ready(pcpu, task, preempt);

		task_clear_resched(task);
		if (task->delay) {
			del_timer(&task->delay_timer);
			task->delay = 0;
		}
	}
	raw_spin_unlock(&pcpu->lock);

	if (need_resched || task_is_idle(current))
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
	struct pcpu *pcpu = get_cpu_var(pcpu);

	pcpu->state = PCPU_STATE_RUNNING;

	request_irq(CONFIG_MINOS_RESCHED_IRQ, resched_handler,
			0, "resched handler", NULL);
	request_irq(CONFIG_MINOS_IRQWORK_IRQ, irqwork_handler,
			0, "irqwork handler", NULL);

	return 0;
}
