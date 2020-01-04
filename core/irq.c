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
#include <minos/irq.h>
#include <minos/mm.h>
#include <config/config.h>
#include <minos/device_id.h>
#include <minos/sched.h>
#include <minos/of.h>

#define NR_PERCPU_IRQS		(CONFIG_NR_PPI_IRQS + CONFIG_NR_SGI_IRQS)
#define PERCPU_IRQ_DESC_SIZE	(NR_PERCPU_IRQS * NR_CPUS)
#define SPI_IRQ_DESC_SIZE	CONFIG_NR_SPI_IRQS
#define SPI_IRQ_BASE		NR_PERCPU_IRQS
#define MAX_IRQ_COUNT		(CONFIG_NR_SPI_IRQS + NR_PERCPU_IRQS)

static struct irq_chip *irq_chip;

static int default_irq_handler(uint32_t irq, void *data);

static struct irq_desc percpu_irq_descs[PERCPU_IRQ_DESC_SIZE] = {
	[0 ... (PERCPU_IRQ_DESC_SIZE - 1)] = {
		default_irq_handler,
	},
};

static struct irq_desc spi_irq_descs[SPI_IRQ_DESC_SIZE] = {
	[0 ... (SPI_IRQ_DESC_SIZE - 1)] = {
		default_irq_handler,
	},
};

void send_sgi(uint32_t sgi, int cpu)
{
	cpumask_t mask;

	if ((cpu < 0) || (cpu >= CONFIG_NR_CPUS))
		return;

	if (sgi >= 16)
		return;

	cpumask_clearall(&mask);
	cpumask_set_cpu(cpu, &mask);

	irq_chip->send_sgi(sgi, SGI_TO_LIST, &mask);
}

static int default_irq_handler(uint32_t irq, void *data)
{
	pr_warn("irq %d is not register\n", irq);
	return 0;
}

static inline int do_handle_host_irq(int cpuid, struct irq_desc *irq_desc)
{
	int ret;

	if (cpuid != irq_desc->affinity) {
		pr_notice("irq %d do not belong to this cpu\n", irq_desc->hno);
		ret =  -EINVAL;
		goto out;
	}

	ret = irq_desc->handler(irq_desc->hno, irq_desc->pdata);
out:
	/*
	 * 1: if the hw irq is to vcpu do not dir it
	 * 2: if the hw irq is to vcpu but failed to send then dir it
	 */
	if (ret || (!test_bit(IRQ_FLAGS_VCPU_BIT, &irq_desc->flags)))
		irq_chip->irq_dir(irq_desc->hno);

	return ret;
}

static inline struct irq_desc *get_irq_desc_cpu(int cpuid, uint32_t irq)
{
	if (irq >= MAX_IRQ_COUNT)
		return NULL;

	if (irq < SPI_IRQ_BASE)
		return &percpu_irq_descs[cpuid * NR_PERCPU_IRQS + irq];

	return &spi_irq_descs[irq - SPI_IRQ_BASE];
}

/*
 * notice, when used this function to get the percpu
 * irqs need to lock the kernel to invoid the thread
 * sched out from this cpu and running on another cpu
 *
 * so usually, percpu irq will handle in kernel contex
 * and not in task context
 */
struct irq_desc *get_irq_desc(uint32_t irq)
{
	return get_irq_desc_cpu(smp_processor_id(), irq);
}

void __irq_enable(uint32_t irq, int enable)
{
	struct irq_desc *irq_desc;

	irq_desc = get_irq_desc(irq);
	if (!irq_desc)
		return;

	/*
	 * some irq controller will directly call its
	 * own function to enable or disable the hw irq
	 * which do not set the bit, so here force to excute
	 * the action
	 */
	spin_lock(&irq_desc->lock);
	if (enable) {
		irq_chip->irq_unmask(irq);
		irq_desc->flags &= ~IRQ_FLAGS_MASKED;
	 } else {
		irq_chip->irq_mask(irq);
		irq_desc->flags |= IRQ_FLAGS_MASKED;
	 }
	spin_unlock(&irq_desc->lock);
}

void irq_clear_pending(uint32_t irq)
{
	if (irq_chip->irq_clear_pending)
		irq_chip->irq_clear_pending(irq);
}

void irq_set_affinity(uint32_t irq, int cpu)
{
	struct irq_desc *irq_desc;

	if (cpu >= NR_CPUS)
		return;

	/* update the hw irq affinity */
	irq_desc = get_irq_desc(irq);
	if (!irq_desc)
		return;

	spin_lock(&irq_desc->lock);
	irq_desc->affinity = cpu;

	if (irq_chip->irq_set_affinity)
		irq_chip->irq_set_affinity(irq, cpu);

	spin_unlock(&irq_desc->lock);
}

void irq_set_type(uint32_t irq, int type)
{
	struct irq_desc *irq_desc;

	irq_desc = get_irq_desc(irq);
	if (!irq_desc)
		return;

	spin_lock(&irq_desc->lock);

	if (type == (irq_desc->flags & IRQ_FLAGS_TYPE_MASK))
		goto out;

	if (irq_chip->irq_set_type)
		irq_chip->irq_set_type(irq, type);

	irq_desc->flags &= ~IRQ_FLAGS_TYPE_MASK;
	irq_desc->flags |= type;

out:
	spin_unlock(&irq_desc->lock);
}

int do_irq_handler(void)
{
	uint32_t irq;
	struct irq_desc *irq_desc;
	int cpuid = smp_processor_id();

	while (1) {
		irq = irq_chip->get_pending_irq();
		if (irq >= BAD_IRQ)
			return 0;

		irq_chip->irq_eoi(irq);

		irq_desc = get_irq_desc_cpu(cpuid, irq);
		if (unlikely(!irq_desc)) {
			pr_err("irq is not actived %d\n", irq);
			irq_chip->irq_dir(irq);
			continue;
		}

		do_handle_host_irq(cpuid, irq_desc);
	}

	return 0;
}

int irq_xlate(struct device_node *node, uint32_t *intspec,
		unsigned int intsize, uint32_t *hwirq, unsigned long *f)
{
	if (irq_chip && irq_chip->irq_xlate)
		return irq_chip->irq_xlate(node, intspec, intsize, hwirq, f);
	else
		pr_warn("WARN - no xlate function for the irqchip\n");

	return -ENOENT;
}

int request_irq_percpu(uint32_t irq, irq_handle_t handler,
		unsigned long flags, char *name, void *data)
{
	int i;
	struct irq_desc *irq_desc;
	unsigned long flag;

	unused_arg(name);

	if ((irq >= NR_PERCPU_IRQS) || !handler)
		return -EINVAL;

	for (i = 0; i < NR_CPUS; i++) {
		irq_desc = get_irq_desc_cpu(i, irq);
		if (!irq_desc)
			continue;

		spin_lock_irqsave(&irq_desc->lock, flag);
		irq_desc->handler = handler;
		irq_desc->pdata = data;
		irq_desc->flags |= flags;
		irq_desc->affinity = i;
		irq_desc->hno = irq;

		/* enable the irq here */
		irq_chip->irq_unmask_cpu(irq, i);
		irq_desc->flags &= ~IRQ_FLAGS_MASKED;

		spin_unlock_irqrestore(&irq_desc->lock, flag);
	}

	return 0;
}

int request_irq(uint32_t irq, irq_handle_t handler,
		unsigned long flags, char *name, void *data)
{
	int type;
	struct irq_desc *irq_desc;
	unsigned long flag;

	unused_arg(name);

	if (!handler)
		return -EINVAL;

	irq_desc = get_irq_desc(irq);
	if (!irq_desc)
		return -ENOENT;

	type = flags & IRQ_FLAGS_TYPE_MASK;
	flags &= ~IRQ_FLAGS_TYPE_MASK;

	spin_lock_irqsave(&irq_desc->lock, flag);
	irq_desc->handler = handler;
	irq_desc->pdata = data;
	irq_desc->flags |= flags;
	irq_desc->hno = irq;

	/* enable the hw irq and set the mask bit */
	irq_chip->irq_unmask(irq);
	irq_desc->flags &= ~IRQ_FLAGS_MASKED;

	if (irq < SPI_IRQ_BASE)
		irq_desc->affinity = smp_processor_id();

	spin_unlock_irqrestore(&irq_desc->lock, flag);

	if (type)
		irq_set_type(irq, type);

	return 0;
}

static void *irqchip_init(struct device_node *node, void *arg)
{
	extern unsigned char __irqchip_start;
	extern unsigned char __irqchip_end;
	void *s, *e;
	struct irq_chip *chip;

	if (node->class != DT_CLASS_IRQCHIP)
		return NULL;

	s = (void *)&__irqchip_start;
	e = (void *)&__irqchip_end;

	chip = (struct irq_chip *)of_device_node_match(node, s, e);
	if (!chip)
		return NULL;

	irq_chip = chip;
	if (chip->init)
		chip->init(node);

	return node;
}

static void of_irq_init(void)
{
	of_iterate_all_node(hv_node, irqchip_init, NULL);
}

int irq_init(void)
{
#ifdef CONFIG_DEVICE_TREE
	of_irq_init();
#endif

	if (!irq_chip)
		panic("can not find the irqchip for system\n");

	/*
	 * now init the irqchip, and in the irq chip
	 * the chip driver need to alloc the irq it
	 * need used in the ssystem
	 */
	if (!irq_chip->get_pending_irq)
		panic("No function to get irq nr\n");

	return 0;
}

int irq_secondary_init(void)
{
	if (irq_chip)
		irq_chip->secondary_init();

	return 0;
}
