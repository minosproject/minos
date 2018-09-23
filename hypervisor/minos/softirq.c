/*
 * Refers to linux/kernel/softirq.c
 *
 * Copyright (C) 1992 Linus Torvalds
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
#include <minos/irq.h>
#include <minos/softirq.h>
#include <minos/smp.h>

DEFINE_PER_CPU(struct list_head [NR_SOFTIRQS], softirq_work_list);
DEFINE_PER_CPU(uint32_t, softirq_pending);

static struct softirq_action softirq_vec[NR_SOFTIRQS];

static inline uint32_t local_softirq_pending(void)
{
	return (get_cpu_var(softirq_pending));
}

static void inline set_softirq_pending(uint32_t value)
{
	get_cpu_var(softirq_pending) = value;
}

void raise_softirq_irqoff(unsigned int nr)
{
	unsigned long softirq_pending = get_cpu_var(softirq_pending);

	softirq_pending |= (1UL << nr);
	get_cpu_var(softirq_pending) = softirq_pending;
}

void raise_softirq(unsigned int nr)
{
	unsigned long flags;

	local_irq_save(flags);
	raise_softirq_irqoff(nr);
	local_irq_restore(flags);
}

void open_softirq(int nr, void (*action)(struct softirq_action *))
{
	softirq_vec[nr].action = action;
}

static void __do_softirq(void)
{
	struct softirq_action *h;
	uint32_t pending;

	pending = local_softirq_pending();

	set_softirq_pending(0);
	//enable_local_irq();
	h = softirq_vec;

	do {
		if (pending & 1) {
			h->action(h);
			h++;
			pending >>= 1;
		}
	} while (pending);
	//disable_local_irq();
}

void do_softirq(void)
{
	uint32_t pending;
	unsigned long flags;

	local_irq_save(flags);

	pending = local_softirq_pending();
	if (pending)
		__do_softirq();

	local_irq_restore(flags);
}

void softirq_init(void)
{
	int cpu;
	int i;

	for_all_cpu(cpu) {
		for (i = 0; i < NR_SOFTIRQS; i++)
			init_list(&get_per_cpu(softirq_work_list[i], cpu));
	}
}

void irq_softirq_exit(void)
{
	if (local_softirq_pending())
		do_softirq();
}
