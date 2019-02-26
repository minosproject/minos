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
#include <minos/virq.h>
#include <minos/of.h>

DEFINE_PER_CPU(struct irq_desc **, sgi_irqs);
DEFINE_PER_CPU(struct irq_desc **, ppi_irqs);

DEFINE_PER_CPU(int, in_interrupt);

static struct irq_chip *irq_chip;
static struct irq_domain *irq_domains[IRQ_DOMAIN_MAX];

static struct irq_desc *alloc_irq_desc(void)
{
	struct irq_desc *irq;

	irq = (struct irq_desc *)zalloc(sizeof(struct irq_desc));
	if (!irq)
		return NULL;

	spin_lock_init(&irq->lock);

	return irq;
}

void send_sgi(uint32_t sgi, int cpu)
{
	cpumask_t mask;

	if ((cpu < 0) || (cpu >= CONFIG_NR_CPUS))
		return;

	if (sgi >= 16)
		return;

	cpumask_clear(&mask);
	cpumask_set_cpu(cpu, &mask);

	irq_chip->send_sgi(sgi, SGI_TO_LIST, &mask);
}

static int do_handle_host_irq(struct irq_desc *irq_desc)
{
	uint32_t cpuid = smp_processor_id();
	int ret;

	if (cpuid != irq_desc->affinity) {
		pr_info("irq %d do not belong to this cpu\n", irq_desc->hno);
		ret =  -EINVAL;
		goto out;
	}

	if (!irq_desc->handler) {
		pr_error("Irq is not register by MINOS\n");
		ret = -EINVAL;
		goto out;
	}

	ret = irq_desc->handler(irq_desc->hno, irq_desc->pdata);
	if (ret)
		pr_debug("handle irq:%d fail in minos\n", irq_desc->hno);

out:
	/*
	 * 1: if the hw irq is to vcpu do not dir it
	 * 2: if the hw irq is to vcpu but failed to send then dir it
	 */
	if (ret || (!test_bit(IRQ_FLAGS_VCPU_BIT, &irq_desc->flags)))
		irq_chip->irq_dir(irq_desc->hno);

	return ret;
}

int register_irq_domain(int type, struct irq_domain_ops *ops)
{
	struct irq_domain *domain;

	if (type >= IRQ_DOMAIN_MAX)
		return -EINVAL;

	domain = (struct irq_domain *)zalloc(sizeof(struct irq_domain));
	if (!domain)
		return -ENOMEM;

	domain->type = type;
	domain->ops = ops;
	irq_domains[type] = domain;

	return 0;
}

static struct irq_desc **spi_alloc_irqs(uint32_t start,
		uint32_t count, int type)
{
	struct irq_desc *desc;
	struct irq_desc **irqs;
	uint32_t size;
	int i;

	size = count * sizeof(struct irq_desc *);
	irqs = (struct irq_desc **)zalloc(size);
	if (!irqs)
		return NULL;

	for (i = 0; i < count; i++) {
		desc = alloc_irq_desc();
		if (!desc) {
			pr_error("No more memory for irq desc\n");
			return irqs;
		}

		desc->hno = i + start;
		irqs[i] = desc;
	}

	return irqs;
}

static struct irq_desc *spi_get_irq_desc(struct irq_domain *d, uint32_t irq)
{
	if ((irq < d->start) || (irq >= (d->start + d->count)))
		return NULL;

	return (d->irqs[irq - d->start]);
}

static int spi_int_handler(struct irq_domain *d, struct irq_desc *irq_desc)
{
	return do_handle_host_irq(irq_desc);
}

struct irq_domain_ops spi_domain_ops = {
	.alloc_irqs = spi_alloc_irqs,
	.get_irq_desc = spi_get_irq_desc,
	.irq_handler = spi_int_handler,
};

static struct irq_desc **local_alloc_irqs(uint32_t start,
		uint32_t count, int type)
{
	struct irq_desc **irqs, **tmp = NULL;
	struct irq_desc *desc;
	uint32_t size;
	uint32_t i, j;
	unsigned long addr;

	/*
	 * each cpu will have its local irqs
	 */
	size = count * sizeof(struct irq_desc *) * CONFIG_NR_CPUS;
	irqs = (struct irq_desc **)zalloc(size);
	if (!irqs)
		return NULL;

	addr = (unsigned long)irqs;
	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		if (type == IRQ_DOMAIN_SGI)
			get_per_cpu(sgi_irqs, i) = (struct irq_desc **)addr;
		else if (type == IRQ_DOMAIN_PPI)
			get_per_cpu(ppi_irqs, i) = (struct irq_desc **)addr;

		addr += count * sizeof(struct irq_desc *);
	}

	/*
	 * alloc a irq_desc for each cpu
	 */
	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		if (type == IRQ_DOMAIN_SGI)
			tmp = get_per_cpu(sgi_irqs, i);
		else if (type == IRQ_DOMAIN_PPI)
			tmp = get_per_cpu(ppi_irqs, i);

		for (j = 0; j < count; j++) {
			desc = alloc_irq_desc();
			if (!desc) {
				pr_error("No more memory for local irq desc\n");
				return irqs;
			}

			desc->hno = start + j;
			desc->affinity = i;
			tmp[j] = desc;
		}
	}

	return irqs;
}

static struct irq_desc *local_get_irq_desc(struct irq_domain *d, uint32_t irq)
{
	struct irq_desc **irqs = NULL;

	if ((irq < d->start) || (irq >= (d->start + d->count)))
		return NULL;

	if (d->type == IRQ_DOMAIN_SGI)
		irqs = get_cpu_var(sgi_irqs);
	else if(d->type == IRQ_DOMAIN_PPI)
		irqs = get_cpu_var(ppi_irqs);

	return irqs[irq - d->start];
}

static int local_int_handler(struct irq_domain *d, struct irq_desc *irq_desc)
{
	return do_handle_host_irq(irq_desc);
}

struct irq_domain_ops local_domain_ops = {
	.alloc_irqs = local_alloc_irqs,
	.get_irq_desc = local_get_irq_desc,
	.irq_handler = local_int_handler,
};

static int irq_domain_create_irqs(struct irq_domain *d,
		uint32_t start, uint32_t cnt)
{
	struct irq_desc **irqs;

	if ((cnt == 0) || (cnt >= 1024)) {
		pr_error("%s: invaild irq cnt %d\n", __func__, cnt);
		return -EINVAL;
	}

	if (d->irqs) {
		pr_error("irq desc table has been created\n");
		return -EINVAL;
	}

	irqs = d->ops->alloc_irqs(start, cnt, d->type);
	if (!irqs)
		return -ENOMEM;

	d->start = start;
	d->count = cnt;
	d->irqs = irqs;

	return 0;
}

static int alloc_irqs(uint32_t start, uint32_t cnt, int type)
{
	int ret;
	struct irq_domain *domain;

	if (type >= IRQ_DOMAIN_MAX)
		return -EINVAL;

	domain = irq_domains[type];
	if (!domain)
		return -ENOENT;

	/*
	 * need to check wheter the irq number is
	 * duplicate in the other domain TBD
	 */
	ret = irq_domain_create_irqs(domain, start, cnt);
	if (ret) {
		pr_error("add domain:%d irqs failed\n", type);
		goto out;
	}

out:
	return ret;
}

int irq_alloc_spi(uint32_t start, uint32_t cnt)
{
	return alloc_irqs(start, cnt, IRQ_DOMAIN_SPI);
}

int irq_alloc_sgi(uint32_t start, uint32_t cnt)
{
	return alloc_irqs(start, cnt, IRQ_DOMAIN_SGI);
}

int irq_alloc_ppi(uint32_t start, uint32_t cnt)
{
	return alloc_irqs(start, cnt, IRQ_DOMAIN_PPI);
}

int irq_alloc_lpi(uint32_t start, uint32_t cnt)
{
	return alloc_irqs(start, cnt, IRQ_DOMAIN_LPI);
}

int irq_alloc_special(uint32_t start, uint32_t cnt)
{
	return alloc_irqs(start, cnt, IRQ_DOMAIN_SPECIAL);
}

static struct irq_domain *get_irq_domain(uint32_t irq)
{
	int i;
	struct irq_domain *domain;

	for (i = 0; i < IRQ_DOMAIN_MAX; i++) {
		domain = irq_domains[i];
		if (!domain)
			continue;

		if ((irq >= domain->start) &&
			(irq < domain->start + domain->count))
			return domain;
	}

	return NULL;
}

struct irq_desc *get_irq_desc(uint32_t irq)
{
	struct irq_domain *domain;

	domain = get_irq_domain(irq);
	if (!domain)
		return NULL;

	return domain->ops->get_irq_desc(domain, irq);
}

static struct irq_desc *get_irq_desc_cpu(uint32_t irq, int cpu)
{
	struct irq_desc **irq_descs;

	if (irq >= NR_LOCAL_IRQS)
		return NULL;

	if (irq >= PPI_IRQ_BASE) {
		irq_descs = get_per_cpu(ppi_irqs, cpu);
		return irq_descs[irq - PPI_IRQ_BASE];
	} else {
		irq_descs = get_per_cpu(sgi_irqs, cpu);
		return irq_descs[irq - SGI_IRQ_BASE];
	}

	return NULL;
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
	if (enable) {
		irq_chip->irq_unmask(irq);
		irq_desc->flags &= ~IRQ_FLAGS_MASKED;
	 } else {
		irq_chip->irq_mask(irq);
		irq_desc->flags |= IRQ_FLAGS_MASKED;
	 }
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

static int do_bad_int(uint32_t irq)
{
	pr_debug("Handle bad irq do nothing %d\n", irq);
	irq_chip->irq_dir(irq);

	return 0;
}

int do_irq_handler(void)
{
	uint32_t irq;
	struct irq_desc *irq_desc;
	struct irq_domain *d;
	int ret = 0;

	while (1) {
		irq = irq_chip->get_pending_irq();
		if (irq == BAD_IRQ)
			return 0;

		d = get_irq_domain(irq);
		if (unlikely(!d)) {
			ret = -ENOENT;
			goto error;
		}

		irq_chip->irq_eoi(irq);

		irq_desc = d->ops->get_irq_desc(d, irq);
		if (unlikely(!irq_desc)) {
			pr_error("irq is not actived %d\n", irq);
			ret = -EINVAL;
			goto error;
		}

		if (d->ops->irq_handler(d, irq_desc))
			pr_warn("handing %d irq failed\n", irq);
	}

	return 0;

error:
	do_bad_int(irq);
	return ret;
}

int request_irq_percpu(uint32_t irq, irq_handle_t handler,
		unsigned long flags, char *name, void *data)
{
	int i;
	struct irq_desc *irq_desc;
	unsigned long flag;

	if (irq >= NR_LOCAL_IRQS)
		return -EINVAL;

	for (i = 0; i < NR_CPUS; i++) {
		irq_desc = get_irq_desc_cpu(irq, i);
		if (!irq_desc)
			continue;

		spin_lock_irqsave(&irq_desc->lock, flag);
		irq_desc->handler = handler;
		irq_desc->pdata = data;
		irq_desc->name = name;
		irq_desc->flags |= flags;

		/* enable the irq here */
		irq_chip->irq_unmask_cpu(irq, i);
		irq_desc->flags &= ~IRQ_FLAGS_MASKED;

		spin_unlock_irqrestore(&irq_desc->lock, flag);
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


int request_irq(uint32_t irq, irq_handle_t handler,
		unsigned long flags, char *name, void *data)
{
	int type;
	struct irq_desc *irq_desc;
	unsigned long flag;

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
	irq_desc->name = name;
	irq_desc->flags |= flags;

	/* enable the hw irq and set the mask bit */
	irq_chip->irq_unmask(irq);
	irq_desc->flags &= ~IRQ_FLAGS_MASKED;

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
	register_irq_domain(IRQ_DOMAIN_SPI, &spi_domain_ops);
	register_irq_domain(IRQ_DOMAIN_SGI, &local_domain_ops);
	register_irq_domain(IRQ_DOMAIN_PPI, &local_domain_ops);
	register_irq_domain(IRQ_DOMAIN_SPECIAL, &spi_domain_ops);

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
