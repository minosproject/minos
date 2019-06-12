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
#include <asm/io.h>
#include <minos/percpu.h>
#include <minos/spinlock.h>
#include <minos/print.h>
#include <minos/errno.h>
#include <minos/vmodule.h>
#include <minos/vcpu.h>
#include <asm/arch.h>
#include <minos/cpumask.h>
#include <minos/irq.h>
#include <minos/virq.h>
#include <minos/of.h>
#include <asm/bcm_irq.h>

extern int bcm_virq_init(unsigned long l1_base, size_t l1_size,
		unsigned long l2_base, size_t l2_size);

static const int reg_pending[] = { 0x00, 0x04, 0x08 };
static const int reg_enable[] = { 0x18, 0x10, 0x14 };
static const int reg_disable[]= { 0x24, 0x1c, 0x20 };

static const int shortcuts[] = {
	7, 9, 10, 18, 19,		/* Bank 1 */
	21, 22, 23, 24, 25, 30		/* Bank 2 */
};

struct armctrl_ic {
	void *base;
	void *pending[NR_BANKS];
	void *enable[NR_BANKS];
	void *disable[NR_BANKS];
	struct irq_domain *domain;
	void *local_base;
};

static struct armctrl_ic intc;
static void *bcm2836_base;

extern int bcm2836_xlate_irq(struct device_node *node,
		uint32_t *intspec, unsigned int intsize,
		uint32_t *hwirq, unsigned long *type);

static void bcm2835_mask_irq(uint32_t irq)
{
	writel_relaxed(HWIRQ_BIT(irq), intc.disable[HWIRQ_BANK(irq)]);
	dsb();
}

static void bcm2835_unmask_irq(uint32_t irq)
{
	writel_relaxed(HWIRQ_BIT(irq), intc.enable[HWIRQ_BANK(irq)]);
	dsb();
}

static uint32_t armctrl_translate_bank(int bank)
{
	uint32_t stat = readl_relaxed(intc.pending[bank]);

	return MAKE_HWIRQ(bank, __ffs(stat));
}

static uint32_t armctrl_translate_shortcut(int bank, u32 stat)
{
	return MAKE_HWIRQ(bank, shortcuts[__ffs(stat >> SHORTCUT_SHIFT)]);
}

static uint32_t bcm2835_get_pending(void)
{
	uint32_t stat = readl_relaxed(intc.pending[0]) & BANK0_VALID_MASK;

	if (stat == 0)
		return BAD_IRQ;
	else if (stat & BANK0_HWIRQ_MASK)
		return MAKE_HWIRQ(0, __ffs(stat & BANK0_HWIRQ_MASK));
	else if (stat & SHORTCUT1_MASK)
		return armctrl_translate_shortcut(1, stat & SHORTCUT1_MASK);
	else if (stat & SHORTCUT2_MASK)
		return armctrl_translate_shortcut(2, stat & SHORTCUT2_MASK);
	else if (stat & BANK1_HWIRQ)
		return armctrl_translate_bank(1);
	else if (stat & BANK2_HWIRQ)
		return armctrl_translate_bank(2);
	else
		BUG();
}

static uint32_t bcm2836_get_pending(void)
{
	int cpu = smp_processor_id();
	uint32_t stat;
	uint32_t irq;

	stat = readl_relaxed(bcm2836_base + LOCAL_IRQ_PENDING0 + 4 * cpu);

	if (stat & BIT(LOCAL_IRQ_MAILBOX0)) {
		void *mailbox0;
		uint32_t mbox_val;

		/*
		 * support 32 IPI, here only use 16 bit to routing
		 * to the sgi interrupt
		 */
		mailbox0 = bcm2836_base + LOCAL_MAILBOX0_CLR0 + 16 * cpu;
		mbox_val = readl_relaxed(mailbox0);
		if (mbox_val == 0)
			return BAD_IRQ;

		irq = __ffs(mbox_val);
		writel_relaxed(1 << irq, mailbox0);
		dsb();

		if (irq >= 16)
			return BAD_IRQ;

		return irq;
	} else if (stat) {
		/*
		 * map other irq except mailbox to PPI as below:
		 * 16	: CNTPSIRQ
		 * 17	: CNTPNSIRQ
		 * 18	: CNTHPIRQ
		 * 19	: CNTVIRQ
		 * 20 - 23 : Mailbox irq
		 * 24	: GPU interrupt
		 * 25	: PMU interrupt
		 * 26	: AXI outstanding interrupt
		 * 27	: Local timer interrupt
		 */
		irq = __ffs(stat) + 16;
		return irq;
	}

	return BAD_IRQ;
}

static void bcm2836_mask_per_cpu_irq(unsigned int reg_offset,
		unsigned int bit, int cpu)
{
	void *reg = bcm2836_base + reg_offset + 4 * cpu;

	writel_relaxed(readl_relaxed(reg) & ~BIT(bit), reg);
	dsb();
}

static void bcm2836_unmask_per_cpu_irq(unsigned int reg_offset,
		unsigned int bit, int cpu)
{
	void *reg = bcm2836_base + reg_offset + 4 * cpu;

	writel_relaxed(readl_relaxed(reg) | BIT(bit), reg);
	dsb();
}

static void inline __bcm2836_mask_irq(uint32_t irq, int cpu)
{
	int offset;

	if (irq >= 32)
		bcm2835_mask_irq(irq);

	/* TBD : sgi always enable */
	if (irq < 16)
		return;

	offset = irq - 16;
	switch (offset) {
	case LOCAL_IRQ_CNTPSIRQ:
	case LOCAL_IRQ_CNTPNSIRQ:
	case LOCAL_IRQ_CNTHPIRQ:
	case LOCAL_IRQ_CNTVIRQ:
		bcm2836_mask_per_cpu_irq(LOCAL_TIMER_INT_CONTROL0, offset, cpu);
		break;

	case LOCAL_IRQ_PMU_FAST:
		writel_relaxed(1 << cpu, bcm2836_base + LOCAL_PM_ROUTING_CLR);
		dsb();
		break;

	case LOCAL_IRQ_GPU_FAST:
		break;
	}
}

static void inline __bcm2836_unmask_irq(uint32_t irq, int cpu)
{
	int offset;

	if (irq >= 32)
		bcm2835_unmask_irq(irq);

	if (irq < 16)
		return;

	offset = irq - 16;
	switch (offset) {
	case LOCAL_IRQ_CNTPSIRQ:
	case LOCAL_IRQ_CNTPNSIRQ:
	case LOCAL_IRQ_CNTHPIRQ:
	case LOCAL_IRQ_CNTVIRQ:
		bcm2836_unmask_per_cpu_irq(LOCAL_TIMER_INT_CONTROL0, offset, cpu);
		break;

	case LOCAL_IRQ_PMU_FAST:
		writel_relaxed(1 << cpu, bcm2836_base + LOCAL_PM_ROUTING_SET);
		dsb();
		break;

	case LOCAL_IRQ_GPU_FAST:
		break;
	}
}

static int bcm2836_set_irq_priority(uint32_t irq, uint32_t pr)
{
	return 0;
}

static void bcm2836_mask_irq(uint32_t irq)
{
	__bcm2836_mask_irq(irq, smp_processor_id());
}

static void bcm2836_unmask_irq(uint32_t irq)
{
	__bcm2836_unmask_irq(irq, smp_processor_id());
}

static void bcm2836_mask_irq_cpu(uint32_t irq, int cpu)
{
	__bcm2836_mask_irq(irq, cpu);
}

static void bcm2836_unmask_irq_cpu(uint32_t irq, int cpu)
{
	__bcm2836_unmask_irq(irq, cpu);
}

static void bcm2836_send_sgi(uint32_t sgi, enum sgi_mode mode, cpumask_t *cpu)
{
	int c;
	void *mailbox0_base = bcm2836_base + LOCAL_MAILBOX0_SET0;

	dsb();
	if (sgi >= 16)
		return;

	switch (mode) {
	case SGI_TO_OTHERS:
		for_each_cpu(c, cpu) {
			if (c == smp_processor_id())
				continue;
			writel_relaxed(1 << sgi, mailbox0_base + 16 * c);
			dsb();
		}
		break;
	case SGI_TO_SELF:
		writel_relaxed(1 << sgi, mailbox0_base +
				16 * smp_processor_id());
		dsb();
		break;
	case SGI_TO_LIST:
		for_each_cpu(c, cpu) {
			writel_relaxed(1 << sgi, mailbox0_base + 16 * c);
			dsb();
		}
		break;
	}
}

static int bcm2836_set_irq_affinity(uint32_t irq, uint32_t pcpu)
{
	return 0;
}

static int bcm2836_set_irq_type(uint32_t irq, uint32_t type)
{
	return 0;
}

static void bcm2836_dir_irq(uint32_t irq)
{
	if (irq >= 32)
		bcm2835_unmask_irq(irq);
}

static void bcm2836_eoi_irq(uint32_t irq)
{
	if (irq >= 32)
		bcm2835_mask_irq(irq);
}

int bcm2835_irq_handler(uint32_t irq, void *data)
{
	uint32_t no;
	struct irq_desc *irq_desc;

	while ((no = bcm2835_get_pending()) != BAD_IRQ) {
		irq_desc = get_irq_desc(no);
		if (!irq_desc || !irq_desc->handler) {
			bcm2835_mask_irq(no);
			pr_error("irq is not register disable it\n", irq);
			continue;
		}

		irq_desc->handler(irq_desc->hno, irq_desc->pdata);

		/*
		 * if the hardware irq is for vm mask it here
		 * until the vm notify that the hardware irq
		 * is handled
		 */
		if (test_bit(IRQ_FLAGS_VCPU_BIT, &irq_desc->flags))
			bcm2835_mask_irq(no);
	}

	return 0;
}

static int bcm2836_irq_init(struct device_node *node)
{
	void *base;
	int b;

	pr_info("boardcom bcm2836 l1 interrupt init\n");

	bcm2836_base = (void *)0x40000000;
	io_remap(0x40000000, 0x40000000, 0x100);

	/*
	 * set the timer to source for the 19.2Mhz crstal clock
	 * and set the timer prescaler to 1:1
	 */
	writel_relaxed(0, bcm2836_base + LOCAL_CONTROL);
	writel_relaxed(0x80000000, bcm2836_base + LOCAL_PRESCALER);

	/*
	 * int rpi-3b there are two irq_chip controller, the
	 * bcm2836 local interrupt controller is percpu and
	 * the bcm2835 is not percpu so :
	 * bcm2836 id  : 0 - 31
	 * bcm2835 id  : 32 - 127
	 */
	irq_alloc_sgi(0, 16);
	irq_alloc_ppi(16, 16);

	/* enable mailbox0 interrupt for each core */
	writel_relaxed(1, bcm2836_base + LOCAL_MAILBOX_INT_CONTROL0);
	writel_relaxed(1, bcm2836_base + LOCAL_MAILBOX_INT_CONTROL0 + 0x4);
	writel_relaxed(1, bcm2836_base + LOCAL_MAILBOX_INT_CONTROL0 + 0x8);
	writel_relaxed(1, bcm2836_base + LOCAL_MAILBOX_INT_CONTROL0 + 0xc);

	/* init the bcm2835 interrupt controller for spi */
	base = intc.base = (void *)0x3f00b200;
	io_remap(0x3f00b200, 0x3f00b200, 0x100);

	for (b = 0; b < NR_BANKS; b++) {
		intc.pending[b] = base + reg_pending[b];
		intc.enable[b] = base + reg_enable[b];
		intc.disable[b] = base + reg_disable[b];
	}

	irq_alloc_spi(32, 96);

	/*
	 * request the irq handler for the bcm2835 inc
	 * TBD - now the hardware irq only route to cpu0
	 */
	request_irq(24, bcm2835_irq_handler, 0, "bcm2835_irq", NULL);

	return 0;
}

static int bcm2836_secondary_init(void)
{
	return 0;
}

static struct irq_chip bcm2836_irq_chip = {
	.irq_mask		= bcm2836_mask_irq,
	.irq_mask_cpu		= bcm2836_mask_irq_cpu,
	.irq_unmask		= bcm2836_unmask_irq,
	.irq_unmask_cpu		= bcm2836_unmask_irq_cpu,
	.irq_eoi		= bcm2836_eoi_irq,
	.irq_dir		= bcm2836_dir_irq,
	.irq_set_type		= bcm2836_set_irq_type,
	.get_pending_irq	= bcm2836_get_pending,
	.irq_set_affinity 	= bcm2836_set_irq_affinity,
	.irq_xlate		= bcm2836_xlate_irq,
	.send_sgi		= bcm2836_send_sgi,
	.irq_set_priority	= bcm2836_set_irq_priority,
	.init			= bcm2836_irq_init,
	.secondary_init		= bcm2836_secondary_init,
};

IRQCHIP_DECLARE(bcm2836_chip, bcmirq_match_table,
		(void *)&bcm2836_irq_chip);
