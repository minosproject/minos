#include <minos/sched.h>
#include <minos/minos.h>
#include <minos/percpu.h>
#include <minos/pm.h>
#include <minos/irq.h>
#include <minos/list.h>
#include <minos/timer.h>
#include <minos/time.h>
#include <minos/os.h>
#include <minos/task.h>
#include <virt/virt.h>

static struct pcpu pcpus[CONFIG_NR_CPUS];

DEFINE_PER_CPU(struct pcpu *, pcpu);
DEFINE_PER_CPU(struct task *, percpu_current_task);
DEFINE_PER_CPU(struct task *, percpu_next_task);

uint8_t const uint8_ffs_table[256] = {
    0u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u,
    4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u,
    5u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u,
    4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u,
    6u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u,
    4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u,
    5u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u,
    4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u,
    7u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u,
    4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u,
    5u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u,
    4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u,
    6u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u,
    4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u,
    5u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u,
    4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u
};

struct task *get_highest_pending_task(struct pcpu *pcpu)
{
	unsigned long x, y;
	uint8_t tmp;

	x = __ffs64(pcpu->ready_group);
	tmp = pcpu->ready_tbl[x];
	y = uint8_ffs_table[tmp];

	return pcpu->task_table[(x << 3) + y];
}

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

}

void sched_new(void)
{
	unsigned long flags;
	struct pcpu *pcpu = get_cpu_var(pcpu);

	spin_lock_irqsave(&pcpu->lock, flags);
	next_task = get_highest_pending_task(pcpu);
	spin_unlock_irqrestore(&pcpu->lock, flags);
}

void sched(void)
{
	struct task *task, *current;
	unsigned long flags;
	struct pcpu *pcpu = get_cpu_var(pcpu);

	spin_lock_irqsave(&pcpu->lock, flags);
	task = get_highest_pending_task(pcpu);
	spin_unlock_irqrestore(&pcpu->lock, flags);

	local_irq_disable();

	current = current_task;

	if (task != current) {
		next_task = task;
		switch_task_sw(current, task);
	}

	local_irq_enable();
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
		init_list(&pcpu->vcpu_list);
		init_list(&pcpu->ready_list);
		pcpu->pcpu_id = i;

		/*
		 * init the sched timer
		 */
		init_timer(&pcpu->sched_timer);
		pcpu->sched_timer.function = sched_timer_function;

		get_per_cpu(pcpu, i) = pcpu;
	}
}

void set_task_ready(struct task *task)
{
	unsigned long flag;
	struct pcpu *pcpu = get_per_cpu(pcpu, task->affinity);

	spin_lock_irqsave(&pcpu->lock, flag);

	/* set the task ready to run */
	atomic_set(&task->task_stat, TASK_STAT_READY);
	atomic_set(&task->task_pend_stat, TASK_STAT_PEND_OK);
	pcpu->ready_group |= (1 << task->bit_map_x);
	pcpu->ready_tbl[task->bit_map_x] |= (1 << task->bit_map_y);

	spin_unlock_irqrestore(&pcpu->lock, flag);
}

void pcpu_add_task(int cpu, struct task *task)
{
	struct pcpu *pcpu;
	unsigned long flag;

	if (cpu >= NR_CPUS) {
		pr_error("No such physical cpu:%d\n", cpu);
		return;
	}

	pcpu = get_per_cpu(pcpu, cpu);

	spin_lock_irqsave(&pcpu->lock, flag);
	if (pcpu->task_table[task->pr]) {
		pr_error("pr:%d of this cpu:%d already exist\n", task->pr, cpu);
		goto out;
	}

	pcpu->task_table[task->pr] = task;
	list_add_tail(&pcpu->task_list, &task->list);
	task->pid = pcpu->pids;
	pcpu->pids++;

out:
	spin_unlock_irqrestore(&pcpu->lock, flag);
}

static int reched_handler(uint32_t irq, void *data)
{
	return 0;
}

int sched_init(void)
{
	struct timer_list *timer;
	struct pcpu *pcpu = get_cpu_var(pcpu);

	request_irq(CONFIG_MINOS_RESCHED_IRQ, reched_handler,
			0, "resched handler", NULL);

	timer = &pcpu->sched_timer;
	timer->expires = NOW() + MILLISECS(CONFIG_SCHED_INTERVAL);
	add_timer(timer);

	return 0;
}
