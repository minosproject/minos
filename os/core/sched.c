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
#include <minos/of.h>

#ifdef CONFIG_VIRT
#include <virt/vm.h>
#endif

DEFINE_SPIN_LOCK(__kernel_lock);

static prio_t os_rdy_grp;
static uint64_t __os_rdy_table;
static uint8_t *os_rdy_table;
prio_t os_highest_rdy[NR_CPUS];
prio_t os_prio_cur[NR_CPUS];

extern struct task *os_task_table[OS_NR_TASKS];

static struct pcpu pcpus[NR_CPUS];

DEFINE_PER_CPU(struct pcpu *, pcpu);
DEFINE_PER_CPU(int, __os_running);

struct task *__current_tasks[NR_CPUS];
struct task *__next_tasks[NR_CPUS];

#define SCHED_CLASS_LOCAL	0
#define SCHED_CLASS_GLOBAL	1
static int pcpu_sched_class[NR_CPUS];

extern void sched_tick_disable(void);
extern void sched_tick_enable(unsigned long exp);

static void inline set_next_task(struct task *task, int cpuid)
{
	__next_tasks[cpuid] = task;
	wmb();
}

void pcpu_resched(int pcpu_id)
{
	send_sgi(CONFIG_MINOS_RESCHED_IRQ, pcpu_id);
}

static void smp_set_task_ready(void *data)
{
	struct task *task = data;
	struct pcpu *pcpu = get_cpu_var(pcpu);

	/*
	 * current is the task, means the task has already
	 * set to ready stat, there may some mistake that
	 * make this case happend, need to check
	 */
	if ((current == task) || !task_is_ready(task)) {
		pr_warn("%s wrong stat %d\n", __func__, task->stat);
		return;
	}

	list_del(&task->stat_list);
	list_add_tail(&pcpu->ready_list, &task->stat_list);
	pcpu->local_rdy_tasks++;

	if (task->delay) {
		del_timer(&task->delay_timer);
		task->delay = 0;
	}

	set_need_resched();
}

static void smp_set_task_suspend(void *data)
{
	struct task *task = data;
	struct pcpu *pcpu = get_cpu_var(pcpu);

	if ((current == task) || task_is_ready(task)) {
		pr_warn("%s wrong stat %d\n", __func__, task->stat);
		return;
	}

	/*
	 * fix me - when the task is pending to wait
	 * a mutex or sem, how to deal ?
	 */
	list_del(&task->stat_list);
	list_add_tail(&pcpu->sleep_list, &task->stat_list);
	pcpu->local_rdy_tasks--;
}

void set_task_ready(struct task *task)
{
	struct pcpu *pcpu;

	/*
	 * when call this function need to ensure :
	 * 1 - kernel sched lock is locked (rt task)
	 * 2 - the interrupt is disabled
	 */
	if (task_is_idle(task))
		return;

	if (task_is_realtime(task)) {
		os_rdy_grp |= task->bity;
		os_rdy_table[task->by] |= task->bitx;
	} else {
		pcpu = get_cpu_var(pcpu);
		if (pcpu->pcpu_id != task->affinity) {
			smp_function_call(task->affinity, smp_set_task_ready,
					(void *)task, 0);
			return;
		}

		list_del(&task->stat_list);
		list_add(&pcpu->ready_list, &task->stat_list);
		pcpu->local_rdy_tasks++;
	}

	if (task->delay) {
		del_timer(&task->delay_timer);
		task->delay = 0;
	}

	set_need_resched();
}

void set_task_sleep(struct task *task)
{
	struct pcpu *pcpu;

	if (unlikely(task_is_idle(task)))
		return;

	if (task_is_realtime(task)) {
		os_rdy_table[task->by] &= ~task->bitx;
		if (os_rdy_table[task->by] == 0)
			os_rdy_grp &= ~task->bity;
	} else {
		pcpu = get_cpu_var(pcpu);
		if (pcpu->pcpu_id != task->affinity) {
			smp_function_call(task->affinity, smp_set_task_suspend,
					(void *)task, 0);
			return;
		}

		list_del(&task->stat_list);
		list_add(&pcpu->sleep_list, &task->stat_list);
		pcpu->local_rdy_tasks--;
	}
}

void set_task_suspend(uint32_t delay)
{
	unsigned long flags = 0;
	struct task *task = get_current_task();

	task_lock_irqsave(task, flags);
	task->delay = delay;
	task->stat |= TASK_STAT_SUSPEND;
	set_task_sleep(task);
	task_unlock_irqrestore(task, flags);

	sched();
}

struct task *get_highest_task(uint8_t group, prio_t *ready)
{
	uint8_t x, y;

	y = ffs_table[group];
	x = ffs_table[ready[y]];

	return os_task_table[(y << 3) + x];
}

static int __used has_high_prio(prio_t prio, prio_t *array, int *map)
{
	int i;

	for (i = 0; i < NR_CPUS; i++) {
		if (map[i] == 1)
			continue;

		if (prio == array[i])
			return 1;
	}

	return 0;
}

static int __used has_current_prio(prio_t prio, prio_t *array, int *map)
{
	int i;

	for (i = 0; i < NR_CPUS; i++) {
		if ((prio == array[i]) && (map[i] == 0))
			return (i + 1);
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
	int i, j = 0, k;
	prio_t rdy_grp;
	prio_t ncpu_highest[NR_CPUS];
	int high_map[NR_CPUS];
	int current_map[NR_CPUS];
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
	memset(ncpu_highest, OS_PRIO_IDLE + 1, sizeof(ncpu_highest));
	memset(high_map, 0, sizeof(high_map));
	memset(current_map, 0, sizeof(current_map));

	for (i = 0; i < NR_CPUS; i++) {
		if (pcpu_sched_class[i] == SCHED_CLASS_LOCAL) {
			current_map[i] = 1;
			high_map[i] = 1;
			continue;
		}

		y = ffs_table[rdy_grp];
		x = ffs_table[rdy_table[y]];
		p = (y << 3) + x;
		ncpu_highest[i] = p;

		/* clear the task ready bit */
		if ((rdy_table[y] &= ~(1 << x)) == 0)
			rdy_grp &= ~(1 << y);

		if (__rdy_table == 0)
			break;
	}

	for (i = 0; i < NR_CPUS; i++) {
		k = has_current_prio(os_prio_cur[i], ncpu_highest, high_map);
		if (k > 0) {
			os_highest_rdy[i] = os_prio_cur[i];
			high_map[k - 1] = 1;
			current_map[i] = 1;
		} else {
			for (j = 0; j < NR_CPUS; j++) {
				if (high_map[j])
					continue;

				if (has_high_prio(ncpu_highest[j],
						os_prio_cur, current_map))
					continue;

				os_highest_rdy[i] = ncpu_highest[j];
				high_map[j] = 1;
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
	os_highest_rdy[0] = OS_PRIO_PCPU;
	if (__os_rdy_table == 0)
		return;

	y = ffs_table[os_rdy_grp];
	x = ffs_table[os_rdy_table[y]];
	p = (y << 3) + x;
	os_highest_rdy[0] = p;
	dsb();
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

static void inline no_task_sched_return(struct task *task)
{
	/*
	 * if need resched but there something block to sched
	 * a new task, then need enable the sched timer, but
	 * only give the task little run time
	 */
	if ((task_info(task)->flags & __TIF_NEED_RESCHED) &&
			task_is_percpu(task) && (task->start_ns == 0)) {
		task->start_ns = NOW();
		sched_tick_enable(MILLISECS(task->run_time));
	}

	task_sched_return(task);
}

static inline struct task *get_next_global_run_task(struct pcpu *pcpu)
{
	prio_t prio = os_highest_rdy[pcpu->pcpu_id];

	if (prio <= OS_LOWEST_PRIO)
		return os_task_table[prio];

	if (!is_list_empty(&pcpu->ready_list))
		return list_first_entry(&pcpu->ready_list,
				struct task, stat_list);

	return pcpu->idle_task;
}

static inline struct task *get_next_local_run_task(struct pcpu *pcpu)
{
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

static inline void
recal_task_run_time(struct task *task, struct pcpu *pcpu, int suspend)
{
	unsigned long now;

	if (!task_is_percpu(task))
		return;

	now = (NOW() - task->start_ns) / 1000000;
	now = now > CONFIG_SCHED_INTERVAL ? 0 : CONFIG_SCHED_INTERVAL - now;

	if (now < 15)
		task->run_time = CONFIG_SCHED_INTERVAL + now;
	else
		task->run_time = now;

	if (!suspend) {
		list_del(&task->stat_list);
		list_add_tail(&pcpu->ready_list, &task->stat_list);
	}
}

static void global_switch_out(struct pcpu *pcpu,
		struct task *cur, struct task *next)
{
	int cpuid = pcpu->pcpu_id;
	prio_t prio;

	/* set the current prio to the highest ready */
	if (task_is_idle(next))
		prio = OS_PRIO_PCPU;
	else
		prio = next->prio;

	os_prio_cur[cpuid] = prio;
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

void switch_to_task(struct task *cur, struct task *next)
{
	struct pcpu *pcpu = get_cpu_var(pcpu);

	/* save the task contex for the current task */
	save_task_context(cur);

	/*
	 * check the current task's stat and do some action
	 * to it, check whether it suspend time is set or not
	 *
	 * if the task is ready state, adjust theurun time of
	 * this task
	 */
	if (!task_is_ready(cur)) {
		recal_task_run_time(cur, pcpu, 1);
		if (cur->delay) {
			mod_timer(&cur->delay_timer,
				NOW() + MILLISECS(cur->delay));
		}
	} else {
		recal_task_run_time(cur, pcpu, 0);
		cur->stat = TASK_STAT_RDY;
	}

	do_hooks((void *)cur, NULL, OS_HOOK_TASK_SWITCH_OUT);
	pcpu->switch_out(pcpu, cur, next);

	/*
	 * if the next running task prio is OS_PRIO_PCPU, it
	 * need to enable the sched timer for fifo task sched
	 * otherwise disable it.
	 */
	next->start_ns = NOW();
	if (task_is_percpu(next))
		sched_tick_enable(MILLISECS(next->run_time));
	else
		sched_tick_disable();

	restore_task_context(next);

	next->stat = TASK_STAT_RUNNING;
	task_info(next)->cpu = pcpu->pcpu_id;

	do_hooks((void *)next, NULL, OS_HOOK_TASK_SWITCH_TO);
	pcpu->switch_to(pcpu, cur, next);

	task_sched_return(next);
}

unsigned long sched_tick_handler(unsigned long data)
{
	struct task *task;
	struct pcpu *pcpu = get_cpu_var(pcpu);
	unsigned long now = NOW();
	unsigned long delta;

	task = get_current_task();
	delta = now - task->start_ns;

	if (task->prio != OS_PRIO_PCPU) {
		pr_warn("wrong task type on tick handler %d\n", task->pid);
		return 0;
	}

	/*
	 * there is a case that when the sched timer has been
	 * expires when switch out task, once switch to a new
	 * task, the interrupt will be triggered, but the old
	 * task is switch out, so directly return, do not switch
	 * to other task
	 */
	if (delta < MILLISECS(task->run_time))
		pr_debug("Bug happend on timer tick sched 0x%p 0x%p %d %d\n",
				now, task->start_ns, task->run_time, delta);

	/*
	 * if the preempt is disable at this time, what will
	 * happend if the task is not on the head of the pcpu's
	 * ready list ? need further check.
	 */
	task->start_ns = 0;
	task->run_time = CONFIG_TASK_RUN_TIME;
	if (task_is_ready(task)) {
		list_del(&task->stat_list);
		list_add_tail(&pcpu->ready_list, &task->stat_list);
	} else
		pr_info("task is not ready now\n");

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
	mb();

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
	mb();

	if (next == cur)
		return;

	set_next_task(next, pcpu->pcpu_id);
	arch_switch_task_sw();
}

void sched(void)
{
	unsigned long flags;
	struct pcpu *pcpu;
	struct task *cur = get_current_task();

	if ((!preempt_allowed())) {
		panic("os can not sched now preempt disabled\n");
		return;
	}

	local_irq_save(flags);
	clear_need_resched();
	pcpu = get_cpu_var(pcpu);
	pcpu->sched(pcpu, cur);
	local_irq_restore(flags);
}

void sched_task(struct task *task)
{
	unsigned long flags;
	int t_cpu, s_cpu, smp_resched = 0;

	/*
	 * check whether this task can be sched on this
	 * cpu, since there may some pcpu do not sched
	 * the realtime task, if the realtime task is
	 * sleep on other cpu need to send a resched signal
	 */
	local_irq_save(flags);
	s_cpu = smp_processor_id();

	if (task_is_realtime(task))
		t_cpu = task_info(task)->cpu;
	else
		t_cpu = task->affinity;

	if ((t_cpu != s_cpu) && pcpu_sched_class[s_cpu] != SCHED_CLASS_GLOBAL)
		smp_resched = 1;

	local_irq_restore(flags);

	if (smp_resched)
		pcpu_resched(t_cpu);
	else
		sched();
}

void irq_enter(gp_regs *regs)
{
	do_hooks(get_current_task(), (void *)regs,
			OS_HOOK_ENTER_IRQ);

#ifdef CONFIG_VIRT
	if (taken_from_guest(regs))
		exit_from_guest(get_current_vcpu(), regs);
#endif
}

static void local_irq_handler(struct pcpu *pcpu, struct task *task)
{
	struct task *next;

	next = get_next_local_run_task(pcpu);
	if (next == task) {
		no_task_sched_return(task);
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

#if 0
	/*
	 * if the highest prio is update by other cpu, then
	 * sched out directly
	 */
	if ((os_prio_cur[cpuid] != os_highest_rdy[cpuid]))
		goto out;
#endif

	sched_new(pcpu);

	for (i = 0; i < NR_CPUS; i++) {
		if (os_prio_cur[i] != os_highest_rdy[i]) {
			if (i != cpuid)
				pcpu_resched(i);
		}
	}

	/*
	 * if need sched or the current task is idle, then
	 * try to get the next task to check whether need
	 * to sched to anther task
	 */
	next = get_next_global_run_task(pcpu);
	mb();
	if (next != task) {
		set_next_task(next, cpuid);
		switch_to_task(task, next);
		mb();

		return;
	}

	kernel_unlock();
	no_task_sched_return(next);
}

void irq_return_handler(struct task *task)
{
	int p, n;
	struct task_info *ti;
	struct pcpu *pcpu = get_cpu_var(pcpu);

	ti = (struct task_info *)task->stack_origin;
	p = ti->preempt_count;
	n = !(ti->flags & __TIF_NEED_RESCHED);

	/*
	 * if the task is suspend state, means next the cpu
	 * will call sched directly, so do not sched out here
	 */
	if (p || n) {
		no_task_sched_return(task);
	} else {
		pcpu->irq_handler(pcpu, task);
		clear_bit(TIF_NEED_RESCHED, &ti->flags);
	}
}

void irq_exit(gp_regs *regs)
{
	irq_softirq_exit();
}

static void __used *of_setup_pcpu(struct device_node *node, void *data)
{
	int cpuid;
	uint64_t affinity;
	struct pcpu *pcpu;
	char class[16];

	if (node->class != DT_CLASS_CPU)
		return NULL;

	if (!of_get_u64_array(node, "reg", &affinity, 1))
		return NULL;

	cpuid = affinity_to_cpuid(affinity);
	if (cpuid >= NR_CPUS)
		return NULL;

	memset(class, 0, 16);
	if (!of_get_string(node, "sched_class", class, 15))
		return NULL;

	pr_info("sched class of pcpu-%d: %s\n", cpuid, class);

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
		init_list(&pcpu->ready_list);
		init_list(&pcpu->sleep_list);
		init_list(&pcpu->new_list);
		init_list(&pcpu->stop_list);
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
}

int sched_init(void)
{
	os_rdy_table = (uint8_t *)&__os_rdy_table;

	return 0;
}

int resched_handler(uint32_t irq, void *data)
{
	struct task *task, *n;
	struct pcpu *pcpu = get_cpu_var(pcpu);

	/*
	 * check whether there new task need to add to
	 * this pcpu
	 */
	raw_spin_lock(&pcpu->lock);
	list_for_each_entry_safe(task, n, &pcpu->new_list, stat_list) {
		list_del(&task->stat_list);
		if (task->stat == TASK_STAT_RDY) {
			list_add_tail(&pcpu->ready_list, &task->stat_list);
		} else {
			list_add_tail(&pcpu->sleep_list, &task->stat_list);
		}
	}
	raw_spin_unlock(&pcpu->lock);

	set_need_resched();

	return 0;
}

int local_sched_init(void)
{
	struct pcpu *pcpu = get_cpu_var(pcpu);

	pcpu->state = PCPU_STATE_RUNNING;

	return request_irq(CONFIG_MINOS_RESCHED_IRQ, resched_handler,
			0, "resched handler", NULL);
}
