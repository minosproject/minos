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
#include <asm/gicv3.h>
#include <minos/errno.h>
#include <minos/vmodule.h>
#include <asm/arch.h>
#include <minos/cpumask.h>
#include <minos/irq.h>
#include <minos/of.h>
#include <minos/mmu.h>

spinlock_t gicv3_lock;
static void *gicd_base = 0;

extern int vgicv3_init(uint64_t *data, int len);

DEFINE_PER_CPU(void *, gicr_rd_base);
DEFINE_PER_CPU(void *, gicr_sgi_base);

#define gicr_rd_base()	get_cpu_var(gicr_rd_base)
#define gicr_sgi_base()	get_cpu_var(gicr_sgi_base)

extern int gic_xlate_irq(struct device_node *node,
		uint32_t *intspec, unsigned int intsize,
		uint32_t *hwirq, unsigned long *type);

static void gicv3_gicd_wait_for_rwp(void)
{
	while (ioread32(gicd_base + GICD_CTLR) & (1 << 31));
}

static void gicv3_gicr_wait_for_rwp(void)
{
	while (ioread32(gicr_rd_base() + GICR_CTLR) & (1 << 31));
}

static void gicv3_eoi_irq(uint32_t irq)
{
	write_sysreg32(irq, ICC_EOIR1_EL1);
	isb();
}

static void gicv3_dir_irq(uint32_t irq)
{
	write_sysreg32(irq, ICC_DIR_EL1);
	isb();
}

static uint32_t gicv3_read_irq(void)
{
	uint32_t irq;

	irq = read_sysreg32(ICC_IAR1_EL1);
	dsbsy();
	return irq;
}

static int gicv3_set_irq_type(uint32_t irq, uint32_t type)
{
	void *base;
	uint32_t cfg, edgebit;

	/* sgi are always edge-triggered */
	if (irq < GICV3_NR_SGI)
		return 0;

	spin_lock(&gicv3_lock);

	if (irq >= GICV3_NR_LOCAL_IRQS)
		base = (void *)gicd_base + GICD_ICFGR + (irq / 16) * 4;
	else
		base = (void *)gicr_sgi_base() + GICR_ICFGR1;

	cfg = ioread32(base);
	edgebit = 2u << (2 * (irq % 16));
	if (type & IRQ_FLAGS_LEVEL_BOTH)
		cfg &= ~edgebit;
	else if (type & IRQ_FLAGS_EDGE_BOTH)
		cfg |= edgebit;

	iowrite32(cfg, base);
	isb();

	spin_unlock(&gicv3_lock);

	return 0;
}

static void gicv3_clear_pending(uint32_t irq)
{
	uint32_t offset, bit;

	spin_lock(&gicv3_lock);

	if (irq < GICV3_NR_LOCAL_IRQS) {
		iowrite32(BIT(irq), (void *)gicr_sgi_base() + GICR_ICPENDR0);
	} else {
		irq = irq - 32;
		offset = irq / 32;
		bit = offset % 32;
		iowrite32(BIT(bit), (void *)gicd_base + \
				GICD_ICPENDR + (offset * 4));
	}

	spin_unlock(&gicv3_lock);
}

static int gicv3_set_irq_priority(uint32_t irq, uint32_t pr)
{
	spin_lock(&gicv3_lock);

	if (irq < GICV3_NR_LOCAL_IRQS)
		iowrite8(pr, gicr_sgi_base() + GICR_IPRIORITYR0 + irq);
	else
		iowrite8(pr, gicd_base + GICD_IPRIORITYR + irq);

	spin_unlock(&gicv3_lock);

	return 0;
}

static int gicv3_set_irq_affinity(uint32_t irq, uint32_t pcpu)
{
	uint64_t affinity;

	affinity = cpuid_to_affinity(pcpu);
	affinity &= ~(1 << 31); //GICD_IROUTER_SPI_MODE_ANY

	spin_lock(&gicv3_lock);
	iowrite64(affinity, gicd_base + GICD_IROUTER + irq * 8);
	spin_unlock(&gicv3_lock);

	return 0;
}

static void gicv3_send_sgi_list(uint32_t sgi, cpumask_t *mask)
{
	int cpu;
	uint64_t val;
	int list_cluster0 = 0;
	int list_cluster1 = 0;

	for_each_cpu(cpu, mask) {
		if (cpu >= CONFIG_NR_CPUS_CLUSTER0)
			list_cluster1 |= 1 << (cpu - CONFIG_NR_CPUS_CLUSTER0);
		else
			list_cluster0 |= (1 << cpu);
	}

	/*
	 * TBD: now only support two cluster
	 */
	if (list_cluster0) {
		val = list_cluster0 | (0ul << 16) | (0ul << 32) |
			(0ul << 48) | (sgi << 24);
		write_sysreg64(val, ICC_SGI1R_EL1);
		isb();
	}

	if (list_cluster1) {
		val = list_cluster1 | (1ul << 16) | (0ul << 32) |
			(0ul << 48) | (sgi << 24);
		write_sysreg64(val, ICC_SGI1R_EL1);
		isb();
	}
}

static void gicv3_send_sgi(uint32_t sgi, enum sgi_mode mode, cpumask_t *cpu)
{
	cpumask_t cpus_mask;

	if (sgi > 15)
		return;

	cpumask_clearall(&cpus_mask);

	switch (mode) {
	case SGI_TO_OTHERS:
		write_sysreg64(ICH_SGI_TARGET_OTHERS << ICH_SGI_IRQMODE_SHIFT |
				(uint64_t)sgi << ICH_SGI_IRQ_SHIFT, ICC_SGI1R_EL1);
		isb();
		break;
	case SGI_TO_SELF:
		cpumask_set_cpu(smp_processor_id(), &cpus_mask);
		gicv3_send_sgi_list(sgi, &cpus_mask);
		break;
	case SGI_TO_LIST:
		gicv3_send_sgi_list(sgi, cpu);
		break;
	default:
		pr_err("Sgi mode not supported\n");
		break;
	}
}

static void gicv3_mask_irq(uint32_t irq)
{
	uint32_t mask = 1 << (irq % 32);

	spin_lock(&gicv3_lock);
	if (irq < GICV3_NR_LOCAL_IRQS) {
		iowrite32(mask, gicr_sgi_base() + GICR_ICENABLER + (irq / 32) * 4);
		gicv3_gicr_wait_for_rwp();
	} else {
		iowrite32(mask, gicd_base + GICD_ICENABLER + (irq / 32) * 4);
		gicv3_gicd_wait_for_rwp();
	}
	spin_unlock(&gicv3_lock);
}

static void gicv3_unmask_irq(uint32_t irq)
{
	uint32_t mask = 1 << (irq % 32);

	spin_lock(&gicv3_lock);

	if (irq < GICV3_NR_LOCAL_IRQS) {
		iowrite32(mask, gicr_sgi_base() + GICR_ISENABLER + (irq / 32) * 4);
		gicv3_gicr_wait_for_rwp();
	} else {
		iowrite32(mask, gicd_base + GICD_ISENABLER + (irq / 32) * 4);
		gicv3_gicd_wait_for_rwp();
	}

	spin_unlock(&gicv3_lock);
}

static void gicv3_mask_irq_cpu(uint32_t irq, int cpu)
{
	void *base;
	uint32_t mask = 1 << (irq % 32);

	if (irq >= GICV3_NR_LOCAL_IRQS)
		return;

	if (cpu >= NR_CPUS)
		return;

	spin_lock(&gicv3_lock);

	base = get_per_cpu(gicr_sgi_base, cpu);
	base = base + GICR_ICENABLER + (irq / 32) * 4;
	iowrite32(mask, base);
	gicv3_gicr_wait_for_rwp();

	spin_unlock(&gicv3_lock);
}

static void gicv3_unmask_irq_cpu(uint32_t irq, int cpu)
{
	void *base;
	uint32_t mask = 1 << (irq % 32);

	if (irq >= GICV3_NR_LOCAL_IRQS)
		return;

	if (cpu >= NR_CPUS)
		return;

	spin_lock(&gicv3_lock);

	base = get_per_cpu(gicr_sgi_base, cpu);
	base = base + GICR_ISENABLER + (irq / 32) * 4;
	iowrite32(mask, base);
	gicv3_gicr_wait_for_rwp();

	spin_unlock(&gicv3_lock);
}

static void gicv3_wakeup_gicr(void)
{
	uint32_t gicv3_waker_value;

	gicv3_waker_value = ioread32(gicr_rd_base() + GICR_WAKER);
	gicv3_waker_value &= ~(GICR_WAKER_PROCESSOR_SLEEP);
	iowrite32(gicv3_waker_value, gicr_rd_base() + GICR_WAKER);

	while ((ioread32(gicr_rd_base() + GICR_WAKER)
			& GICR_WAKER_CHILDREN_ASLEEP) != 0);
}

static int gicv3_gicc_init(void)
{
	unsigned long reg_value;

	/* enable sre */
	reg_value = read_icc_sre_el2();
	reg_value |= (1 << 0);
	write_icc_sre_el2(reg_value);

	write_sysreg32(0, ICC_BPR1_EL1);
	write_sysreg32(0xff, ICC_PMR_EL1);
	write_sysreg32(1 << 1, ICC_CTLR_EL1);
	write_sysreg32(1, ICC_IGRPEN1_EL1);
	isb();

	return 0;
}

static int gicv3_hyp_init(void)
{
	write_sysreg32(GICH_VMCR_VENG1 | (0xff << 24), ICH_VMCR_EL2);
	write_sysreg32(GICH_HCR_EN, ICH_HCR_EL2);
	isb();

	return 0;
}

static int gicv3_gicr_init(void)
{
	int i;
	uint64_t pr;

	gicv3_wakeup_gicr();

	/* set the priority on PPI and SGI */
	pr = (0x90 << 24) | (0x90 << 16) | (0x90 << 8) | 0x90;
	for (i = 0; i < GICV3_NR_SGI; i += 4)
		iowrite32(pr, gicr_sgi_base() + GICR_IPRIORITYR0 + (i / 4) * 4);

	pr = (0xa0 << 24) | (0xa0 << 16) | (0xa0 << 8) | 0xa0;
	for (i = GICV3_NR_SGI; i < GICV3_NR_LOCAL_IRQS; i += 4)
		iowrite32(pr, gicr_sgi_base() + GICR_IPRIORITYR0 + (i / 4) * 4);

	/* disable all PPI and enable all SGI */
	iowrite32(0xffff0000, gicr_sgi_base() + GICR_ICENABLER);
	iowrite32(0x0000ffff, gicr_sgi_base() + GICR_ISENABLER);

	/* configure SGI and PPI as non-secure Group-1 */
	iowrite32(0xffffffff, gicr_sgi_base() + GICR_IGROUPR0);

	gicv3_gicr_wait_for_rwp();
	isb();

	return 0;
}

int gicv3_init(struct device_node *node)
{
	int i;
	uint32_t type;
	uint32_t nr_lines, nr_pr;
	void *rbase;
	uint64_t pr;
	uint32_t value;
	uint64_t array[10];
	void *__gicr_rd_base = 0;

	pr_info("*** gicv3 init ***\n");

	memset(array, 0, sizeof(array));
	translate_device_address_index(node, &array[0], &array[1], 0);
	translate_device_address_index(node, &array[2], &array[3], 1);
	translate_device_address_index(node, &array[4], &array[5], 2);
	translate_device_address_index(node, &array[6], &array[7], 3);

	spin_lock_init(&gicv3_lock);

	/* only map gicd and gicr now */
	io_remap((vir_addr_t)array[0], (phy_addr_t)array[0],
			(size_t)array[1]);
	io_remap((vir_addr_t)array[2], (phy_addr_t)array[2],
			(size_t)array[3]);

	gicd_base = (void *)(unsigned long)array[0];
	__gicr_rd_base = (void *)(unsigned long)array[2];

	pr_info("gicv3 gicd@0x%x gicr@0x%x\n", (unsigned long)gicd_base,
			(unsigned long)__gicr_rd_base);

	value = read_sysreg32(ICH_VTR_EL2);
	nr_pr = ((value >> 29) & 0x7) + 1;

	if (!((nr_pr > 4) && (nr_pr < 8)))
		panic("GICv3: Invalid number of priority bits\n");

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		rbase = __gicr_rd_base + (128 * 1024) * i;
		get_per_cpu(gicr_rd_base, i) = rbase;
		get_per_cpu(gicr_sgi_base, i) = rbase + (64 * 1024);
		isb();
	}

	spin_lock(&gicv3_lock);

	/* disable gicd */
	iowrite32(0, gicd_base + GICD_CTLR);

	type = ioread32(gicd_base + GICD_TYPER);
	nr_lines = 32 * ((type & 0x1f));
	pr_info("gicv3 typer-0x%x nr_lines-%d\n", type, nr_lines);

	/* alloc LOCAL_IRQS for each cpus */
	irq_alloc_sgi(0, 16);
	irq_alloc_ppi(16, 16);

	/* alloc SPI irqs */
	irq_alloc_spi(32, nr_lines);

	/* default all golbal IRQS to level, active low */
	for (i = GICV3_NR_LOCAL_IRQS; i < nr_lines; i += 16)
		iowrite32(0, gicd_base + GICD_ICFGR + (i / 16) * 4);

	/* default priority for global interrupts */
	for (i = GICV3_NR_LOCAL_IRQS; i < nr_lines; i += 4) {
		pr = (0xa0 << 24) | (0xa0 << 16) | (0xa0 << 8) | 0xa0;
		iowrite32(pr, gicd_base + GICD_IPRIORITYR + (i / 4) * 4);
		pr = ioread32(gicd_base + GICD_IPRIORITYR + (i / 4) * 4);
	}

	/* disable all global interrupt */
	for (i = GICV3_NR_LOCAL_IRQS; i < nr_lines; i += 32)
		iowrite32(0xffffffff, gicd_base + GICD_ICENABLER + (i / 32) *4);

	/* configure SPIs as non-secure GROUP-1 */
	for (i = GICV3_NR_LOCAL_IRQS; i < nr_lines; i += 32)
		iowrite32(0xffffffff, gicd_base + GICD_IGROUPR + (i / 32) *4);

	gicv3_gicd_wait_for_rwp();

	/* enable the gicd */
	iowrite32(1 | GICD_CTLR_ENABLE_GRP1 | GICD_CTLR_ENABLE_GRP1A |
			GICD_CTLR_ARE_NS, gicd_base + GICD_CTLR);
	isb();

	gicv3_gicr_init();
	gicv3_gicc_init();
	gicv3_hyp_init();

	spin_unlock(&gicv3_lock);

#if defined CONFIG_VIRQCHIP_VGICV3 && defined CONFIG_VIRT
	vgicv3_init(array, 10);
#endif

	return 0;
}

int gicv3_secondary_init(void)
{
	spin_lock(&gicv3_lock);

	gicv3_gicr_init();
	gicv3_gicc_init();
	gicv3_hyp_init();

	spin_unlock(&gicv3_lock);

	return 0;
}

static struct irq_chip gicv3_chip = {
	.irq_mask 		= gicv3_mask_irq,
	.irq_mask_cpu		= gicv3_mask_irq_cpu,
	.irq_unmask 		= gicv3_unmask_irq,
	.irq_unmask_cpu 	= gicv3_unmask_irq_cpu,
	.irq_eoi 		= gicv3_eoi_irq,
	.irq_dir		= gicv3_dir_irq,
	.irq_set_type 		= gicv3_set_irq_type,
	.irq_set_affinity 	= gicv3_set_irq_affinity,
	.send_sgi		= gicv3_send_sgi,
	.get_pending_irq	= gicv3_read_irq,
	.irq_clear_pending	= gicv3_clear_pending,
	.irq_set_priority	= gicv3_set_irq_priority,
	.irq_xlate		= gic_xlate_irq,
	.init			= gicv3_init,
	.secondary_init		= gicv3_secondary_init,
};

IRQCHIP_DECLARE(gicv3_chip, gicv3_match_table, (void *)&gicv3_chip);
