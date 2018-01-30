#include <core/types.h>
#include <core/io.h>
#include <core/percpu.h>
#include <asm/armv8.h>
#include <core/gicv3.h>
#include <core/spinlock.h>
#include <core/gic.h>

spinlock_t gic_lock;
static unsigned long gicd_base = 0x2f000000;
static unsigned long __gicr_rd_base = 0x2f100000;

DEFINE_PER_CPU(unsigned long, gicr_rd_base);
DEFINE_PER_CPU(unsigned long, gicr_sgi_base);

#define gicr_rd_base()	get_cpu_var(gicr_rd_base)
#define gicr_sgi_base()	get_cpu_var(gicr_sgi_base)

static void gic_gicd_wait_for_rwp(void)
{
	while (ioread32(gicd_base + GICD_CTLR) & (1 << 31));
}

static void gic_gicr_wait_for_rwp(void)
{
	while (ioread32(gicr_rd_base() + GICR_CTLR) & (1 << 3));
}

void gic_mask_irq(uint32_t irq)
{
	uint32_t mask = 1 << (irq % 32);

	if (irq < GICV3_NR_LOCAL_IRQS) {
		iowrite32(gicr_sgi_base() + GICR_ICENABLER + (irq / 32) * 4, mask);
		gic_gicr_wait_for_rwp();
	} else {
		spin_lock(&gic_lock);
		iowrite32(gicd_base + GICD_ICENABLER + (irq / 32) * 4, mask);
		gic_gicd_wait_for_rwp();
		spin_unlock(&gic_lock);
	}
}

void gic_eoi_irq(uint32_t irq)
{
	WRITE_SYSREG32(irq, ICC_EOIR1_EL1);
	isb();
}

void gic_dir_irq(uint32_t irq)
{
	WRITE_SYSREG32(irq, ICC_DIR_EL1);
	isb();
}

uint32_t gic_read_irq(void)
{
	uint32_t irq;

	irq = READ_SYSREG32(ICC_IAR1_EL1);
	dsbsy();
	return irq;
}

void gic_set_irq_type(uint32_t irq, uint32_t type)
{
	void *base;
	uint32_t cfg, actual, edgebit;

	spin_lock(&gic_lock);

	if (irq > GICV3_NR_LOCAL_IRQS)
		base = (void *)gicd_base + GICD_ICFGR + (irq / 16) * 4;
	else
		base = (void *)gicr_sgi_base() + GICR_ICFGR1;

	cfg = ioread32(base);
	edgebit = 2u << (2 * (irq % 16));
	if (type & IRQ_TYPE_LEVEL_MASK)
		cfg &= ~edgebit;
	else if (type & IRQ_TYPE_EDGE_BOTH)
		cfg |= edgebit;

	iowrite32(base, cfg);

	spin_unlock(&gic_lock);
}

void gic_set_irq_priority(uint32_t irq, uint32_t pr)
{
	spin_lock(&gic_lock);

	if (irq < GICV3_NR_LOCAL_IRQS)
		iowrite32(gicr_sgi_base() + GICR_IPRIORITYR0 + irq, pr);
	else
		iowrite32(gicd_base + GICD_IPRIORITYR + irq, pr);

	spin_unlock(&gic_lock);
}

void gic_unmask_irq(uint32_t irq)
{
	uint32_t mask = 1 << (irq % 32);

	if (irq < GICV3_NR_LOCAL_IRQS) {
		iowrite32(gicr_sgi_base() + GICR_ISENABLER + (irq / 32) * 4, mask);
		gic_gicr_wait_for_rwp();
	} else {
		spin_lock(&gic_lock);
		iowrite32(gicd_base + GICD_ISENABLER + (irq / 32) * 4, mask);
		gic_gicd_wait_for_rwp();
		spin_unlock(&gic_lock);
	}
}

static void gic_wakeup_gicr(void)
{
	uint32_t gic_waker_value;

	gic_waker_value = ioread32(gicr_rd_base() + GICR_WAKER);
	gic_waker_value &= ~(GICR_WAKER_PROCESSOR_SLEEP);
	iowrite32(gicr_rd_base() + GICR_WAKER, gic_waker_value);

	while ((ioread32(gicr_rd_base() + GICR_WAKER)
			& GICR_WAKER_CHILDREN_ASLEEP) != 0);
}

static int gic_percpu_init(void)
{
	int i;
	uint64_t pr;
	uint64_t reg_value;

	spin_lock(&gic_lock);

	gic_wakeup_gicr();

	/* set the priority on PPI and SGI */
	pr = (0x90 << 24) | (0x90 << 16) | (0x90 << 8) | 0x90;
	for (i = 0; i < GICV3_NR_SGI; i++)
		iowrite32(gicr_sgi_base() + GICR_IPRIORITYR0 + (i / 4) * 4, pr);

	pr = (0xa0 << 24) | (0xa0 << 16) | (0xa0 << 8) | 0xa0;
	for (i = GICV3_NR_SGI; i < GICV3_NR_LOCAL_IRQS; i += 4)
		iowrite32(gicr_sgi_base() + GICR_IPRIORITYR0 + (i / 4) * 4, pr);

	/* disable all PPI and enable all SGI */
	iowrite32(gicr_sgi_base() + GICR_ICENABLER, 0xffff0000);
	iowrite32(gicr_sgi_base() + GICR_ICENABLER, 0x0000ffff);

	/* configure SGI and PPI as non-secure Group-1 */
	iowrite32(gicr_sgi_base() + GICR_IGROUPR0, 0xffffffff);

	gic_gicr_wait_for_rwp();

	/* enable sre */
	reg_value = read_icc_sre_el2();
	reg_value |= (1 << 0);
	write_icc_sre_el2(reg_value);

	WRITE_SYSREG32(0, ICC_BPR1_EL1);
	WRITE_SYSREG32(0xff, ICC_PMR_EL1);
	WRITE_SYSREG32(1 << 1, ICC_CTLR_EL1);
	WRITE_SYSREG32(1, ICC_IGRPEN1_EL1);
	isb();

	spin_unlock(&gic_lock);

	return 0;
}

void gic_init(void)
{
	int i;
	uint32_t type;
	uint32_t nr_lines;
	unsigned long rbase;
	uint64_t pr;

	for (i = 0; i < CONFIG_NUM_OF_CPUS; i++) {
		rbase = __gicr_rd_base + (128 * 1024) * i;
		get_per_cpu(gicr_rd_base, i) = rbase;
		get_per_cpu(gicr_sgi_base, i) = rbase + (64 * 1024);
	}

	spin_lock_init(&gic_lock);

	/* disable gicd */
	iowrite32(gicd_base + GICD_CTLR, 0);

	type = ioread32(gicd_base + GICD_TYPER);
	nr_lines = 32 * ((type & 0x1f));

	/* default all golbal IRQS to level, active low */
	for (i = GICV3_NR_LOCAL_IRQS; i < nr_lines; i += 16)
		iowrite32(gicd_base + GICD_ICFGR + (i / 16) * 4, 0);

	/* default priority for global interrupts */
	for (i = GICV3_NR_LOCAL_IRQS; i < nr_lines; i += 4) {
		pr = (0xa0 << 24) | (0xa0 << 16) | (0xa0 << 8) | 0xa0;
		iowrite32(gicd_base + GICD_IPRIORITYR + (i / 4) * 4, pr);
	}

	/* disable all global interrupt */
	for (i = GICV3_NR_LOCAL_IRQS; i < nr_lines; i += 32)
		iowrite32(gicd_base + GICD_ICENABLER + (i / 32) *4, 0xffffffff);

	/* configure SPIs as non-secure GROUP-1 */
	for (i = GICV3_NR_LOCAL_IRQS; i < nr_lines; i += 32)
		iowrite32(0xffffffff, gicd_base + GICD_IGROUPR + (i / 32) *4);

	gic_gicd_wait_for_rwp();

	/* enable the gicd */
	iowrite32(gicd_base + GICD_CTLR, 1 | GICD_CTLR_ENABLE_GRP1 |
			GICD_CTLR_ENABLE_GRP1A | GICD_CTLR_ARE_NS);
	isb();

	gic_percpu_init();
}

void gic_secondary_init(void)
{
	gic_percpu_init();
}
