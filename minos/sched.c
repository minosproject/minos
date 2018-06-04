#include <minos/sched.h>
#include <minos/minos.h>
#include <minos/percpu.h>
#include <minos/pm.h>
#include <minos/irq.h>
#include <minos/list.h>
#include <minos/timer.h>
#include <minos/time.h>
#include <minos/task.h>
#include <virt/virt.h>

static struct pcpu pcpus[CONFIG_NR_CPUS];

DEFINE_PER_CPU(struct pcpu *, pcpu);
DEFINE_PER_CPU(struct task *, percpu_current_task);
DEFINE_PER_CPU(struct task *, percpu_next_task);

#if 0
struct task *get_highest_pending_task(struct pcpu *pcpu)
{
	unsigned long x, y;
	uint8_t tmp;

	x = __ffs64(pcpu->ready_group);
	tmp = pcpu->ready_tbl[x];
	y = uint8_ffs_table[tmp];

	return pcpu->task_table[(x << 3) + y];
}
#endif

void pcpu_resched(int pcpu_id)
{
	send_sgi(CONFIG_MINOS_RESCHED_IRQ, pcpu_id);
}

void switch_to_task(struct task *current, struct task *next)
{
	if (current->task_type == TASK_TYPE_VCPU)
		save_vcpu_task_state(current);

	if (next->task_type == TASK_TYPE_VCPU)
		restore_vcpu_task_state(next);
}

void switch_task_sw(struct task *c, struct task *n)
{
	switch_to_task(c, n);
	arch_switch_task_sw();
}

void sched_task(struct task *task, int reason)
{
	struct pcpu *pcpu = get_cpu_var(pcpu);

	pcpu->sched_class->sched_task(pcpu, task);
}

void sched_new(void)
{
	struct pcpu *pcpu = get_cpu_var(pcpu);

	next_task = pcpu->sched_class->sched_new(pcpu);
}

void sched(void)
{
	unsigned long flags;
	struct task *task, *current;
	struct pcpu *pcpu = get_cpu_var(pcpu);

	task = pcpu->sched_class->pick_task(pcpu);

	local_irq_save(flags);

	current = current_task;

	if (task != current) {
		pcpu->sched_class->sched(pcpu, current, task);
		next_task = task;
		switch_task_sw(current, task);
	}

	local_irq_restore(flags);
}

static void sched_timer_function(unsigned long data)
{

}

void pcpus_init(void)
{
	int i;
	struct pcpu *pcpu;

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		pcpu = &pcpus[i];
		pcpu->state = PCPU_STATE_RUNNING;
		init_list(&pcpu->task_list);
		pcpu->pcpu_id = i;

		/*
		 * init the sched timer
		 */
		init_timer(&pcpu->sched_timer);
		pcpu->sched_timer.function = sched_timer_function;

		get_per_cpu(pcpu, i) = pcpu;
	}
}

static inline void set_task_state(struct task *task, int state)
{
	struct pcpu *pcpu = get_per_cpu(pcpu, task->affinity);

	/* set the task ready to run */
	pcpu->sched_class->set_task_state(pcpu, task, state);
}

void set_task_ready(struct task *task)
{
	set_task_state(task, TASK_STAT_READY);
}

void set_task_suspend(struct task *task)
{
	set_task_state(task, TASK_STAT_READY);
}

int pcpu_add_task(int cpu, struct task *task)
{
	struct pcpu *pcpu;

	if (cpu >= NR_CPUS) {
		pr_error("No such physical cpu:%d\n", cpu);
		return -EINVAL;
	}

	pcpu = get_per_cpu(pcpu, cpu);

	/* init the task's sched private data */
	pcpu->sched_class->init_task_data(pcpu,  task);

	return pcpu->sched_class->add_task(pcpu, task);
}

static int reched_handler(uint32_t irq, void *data)
{
	return 0;
}

int sched_init(void)
{
	int i;
	struct pcpu *pcpu;

	for (i = 0; i < NR_CPUS; i++) {
		pcpu = get_per_cpu(pcpu, i);
		pcpu->sched_class = get_sched_class("fifo");
		pcpu->sched_class->init_pcpu_data(pcpu);
	}

	return 0;
}

int local_sched_init(void)
{
	struct timer_list *timer;
	struct pcpu *pcpu = get_cpu_var(pcpu);

	pcpu->sched_class = get_sched_class("fifo");
	pcpu->sched_class->init_pcpu_data(pcpu);

	request_irq(CONFIG_MINOS_RESCHED_IRQ, reched_handler,
			0, "resched handler", NULL);

	timer = &pcpu->sched_timer;
	timer->expires = NOW() + MILLISECS(CONFIG_SCHED_INTERVAL);
	add_timer(timer);

	return 0;
}
