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

#include <minos/sched.h>
#include <minos/minos.h>
#include <minos/percpu.h>
#include <minos/pm.h>
#include <minos/irq.h>
#include <minos/list.h>
#include <minos/timer.h>
#include <minos/time.h>
#include <minos/virt.h>
#include <minos/virq.h>

static struct pcpu pcpus[CONFIG_NR_CPUS];

DEFINE_PER_CPU(struct pcpu *, pcpu);
DEFINE_PER_CPU(struct vcpu *, percpu_current_vcpu);
DEFINE_PER_CPU(struct vcpu *, percpu_next_vcpu);

DEFINE_PER_CPU(int, need_resched);

void pcpu_resched(int pcpu_id)
{
	send_sgi(CONFIG_MINOS_RESCHED_IRQ, pcpu_id);
}

void switch_to_vcpu(struct vcpu *current, struct vcpu *next)
{
	struct pcpu *pcpu = get_cpu_var(pcpu);

	if (current != next) {
		if (!current->is_idle)
			save_vcpu_vcpu_state(current);

		if (!next->is_idle)
			restore_vcpu_vcpu_state(next);

		pcpu->sched_class->sched(pcpu, current, next);

		current->state = VCPU_STAT_READY;
		next->state = VCPU_STAT_RUNNING;
	}

	if (!next->is_idle)
		enter_to_guest(next, NULL);
}

void sched_vcpu(struct vcpu *vcpu)
{
	struct vcpu *current = current_vcpu;
	struct pcpu *pcpu = get_cpu_var(pcpu);

	/* Fix me TBD */

	if (vcpu == current)
		return;

	if (vcpu_has_hwirq_pending(current)) {
		if ((vcpu->state == VCPU_STAT_SUSPEND) ||
			(vcpu->state == VCPU_STAT_IDLE))
			;
		else
			return;
	}

	if ((vcpu->is_idle) || (vcpu->state == VCPU_STAT_RUNNING))
		return;

	if (vcpu->affinity != current->affinity) {
		vcpu->resched = 1;
		pcpu_resched(vcpu->affinity);
		return;
	}

	if (0) {
		if (in_interrupt)
			need_resched = 1;
		else
			sched();
	}
}

void sched_new(void)
{
	struct pcpu *pcpu = get_cpu_var(pcpu);

	next_vcpu = pcpu->sched_class->pick_vcpu(pcpu);
}

void sched(void)
{
	unsigned long flags;
	struct vcpu *vcpu, *current = current_vcpu;
	struct pcpu *pcpu;

	pcpu = get_cpu_var(pcpu);

	local_irq_save(flags);
	vcpu = pcpu->sched_class->pick_vcpu(pcpu);
	local_irq_restore(flags);

	if ((vcpu != current) && (!need_resched)) {
		local_irq_save(flags);
		pcpu->sched_class->sched(pcpu, current, vcpu);
		switch_to_vcpu(current, vcpu);
		next_vcpu = vcpu;
		dsb();
		arch_switch_vcpu_sw();
		local_irq_restore(flags);
	}
}

void pcpus_init(void)
{
	int i;
	struct pcpu *pcpu;

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		pcpu = &pcpus[i];
		pcpu->state = PCPU_STATE_RUNNING;
		init_list(&pcpu->vcpu_list);
		pcpu->pcpu_id = i;
		get_per_cpu(pcpu, i) = pcpu;
	}
}

void set_vcpu_state(struct vcpu *vcpu, int state)
{
	struct pcpu *pcpu = get_per_cpu(pcpu, vcpu->affinity);

	/* set the vcpu ready to run */
	pcpu->sched_class->set_vcpu_state(pcpu, vcpu, state);
}


int pcpu_add_vcpu(int cpu, struct vcpu *vcpu)
{
	struct pcpu *pcpu;

	if (cpu >= NR_CPUS) {
		pr_error("No such physical cpu:%d\n", cpu);
		return -EINVAL;
	}

	pcpu = get_per_cpu(pcpu, cpu);

	/* init the vcpu's sched private data */
	pcpu->sched_class->init_vcpu_data(pcpu, vcpu);
	list_add_tail(&pcpu->vcpu_list, &vcpu->list);

	return pcpu->sched_class->add_vcpu(pcpu, vcpu);
}

static int resched_handler(uint32_t irq, void *data)
{
	struct pcpu *pcpu = get_cpu_var(pcpu);
	struct vcpu *vcpu;

	list_for_each_entry(vcpu, &pcpu->vcpu_list, list) {
		if (vcpu->resched)
			set_vcpu_state(vcpu, VCPU_STAT_READY);
	}

	need_resched = 1;

	return 0;
}

unsigned long sched_tick_handler(unsigned long data)
{
	struct pcpu *pcpu = get_cpu_var(pcpu);

	return pcpu->sched_class->tick_handler(pcpu);
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
	return request_irq(CONFIG_MINOS_RESCHED_IRQ, resched_handler,
			0, "resched handler", NULL);
}
