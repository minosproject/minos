#include <core/types.h>
#include <core/io.h>
#include <core/percpu.h>
#include <asm/armv8.h>
#include <core/gicv3.h>
#include <core/spinlock.h>
#include <core/gic.h>
#include <core/irq.h>
#include <core/print.h>

spinlock_t gic_lock;
static void *gicd_base = (void *)0x2f000000;
static void * __gicr_rd_base = (void *)0x2f100000;

DEFINE_PER_CPU(void *, gicr_rd_base);
DEFINE_PER_CPU(void *, gicr_sgi_base);

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

void gic_mask_irq(struct vmm_irq *vmm_irq)
{
	uint32_t irq = vmm_irq->hno;
	uint32_t mask = 1 << (irq % 32);

	spin_lock(&gic_lock);
	if (irq < GICV3_NR_LOCAL_IRQS) {
		iowrite32(gicr_sgi_base() + GICR_ICENABLER + (irq / 32) * 4, mask);
		gic_gicr_wait_for_rwp();
	} else {
		iowrite32(gicd_base + GICD_ICENABLER + (irq / 32) * 4, mask);
		gic_gicd_wait_for_rwp();
	}

	vmm_irq->flags &= ~IRQ_STATUS_MASK;
	vmm_irq->flags |= IRQ_STATUS_MASKED;
	spin_unlock(&gic_lock);
}

void gic_eoi_irq(struct vmm_irq *vmm_irq)
{
	uint32_t irq = vmm_irq->hno;

	write_sysreg32(irq, ICC_EOIR1_EL1);
	isb();
}

void gic_dir_irq(struct vmm_irq *vmm_irq)
{
	uint32_t irq = vmm_irq->hno;

	write_sysreg32(irq, ICC_DIR_EL1);
	isb();
}

uint32_t gic_read_irq(void)
{
	uint32_t irq;

	irq = read_sysreg32(ICC_IAR1_EL1);
	dsbsy();
	return irq;
}

void gic_set_irq_type(struct vmm_irq *vmm_irq, uint32_t type)
{
	void *base;
	uint32_t irq = vmm_irq->hno;
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
	vmm_irq->flags &= ~IRQ_TYPE_MASK;
	vmm_irq->flags |= (type & IRQ_TYPE_MASK);

	spin_unlock(&gic_lock);
}

void gic_set_irq_priority(struct vmm_irq *vmm_irq, uint32_t pr)
{
	uint32_t irq = vmm_irq->hno;

	spin_lock(&gic_lock);

	if (irq < GICV3_NR_LOCAL_IRQS)
		iowrite32(gicr_sgi_base() + GICR_IPRIORITYR0 + irq, pr);
	else
		iowrite32(gicd_base + GICD_IPRIORITYR + irq, pr);

	spin_unlock(&gic_lock);
}

void gic_set_irq_affinity(struct vmm_irq *vmm_irq, int cpu)
{

}

static void gic_send_sgi_list(uint32_t sgi, uint32_t *mask)
{
	int i;
	uint64_t list = 0;
	uint64_t val;

	for (i = 0; i < CONFIG_NUM_OF_CPUS; i++) {
		if ((1 << i) & (*mask))
			list |= (1 << i);
	}

	/*
	 * TBD: now only support one cluster
	 */
	val = list | (0ul << 16) | (0ul << 32) |
		(0ul << 48) | (sgi << 24);
	write_sysreg64(val, ICC_SGI1R_EL1);
	isb();
}

void gic_send_sgi(uint32_t sgi, enum sgi_mode mode, uint32_t *cpu_mask)
{
	uint32_t cpus_mask;

	if (sgi > 15)
		return;

	switch (mode) {
	case SGI_TO_OTHERS:
		write_sysreg64(ICH_SGI_TARGET_OTHERS << ICH_SGI_IRQMODE_SHIFT |
				(uint64_t)sgi << ICH_SGI_IRQ_SHIFT, ICC_SGI1R_EL1);
		isb();
		break;
	case SGI_TO_SELF:
		cpus_mask |= (1 << get_cpu_id());
		gic_send_sgi_list(sgi, &cpus_mask);
		break;
	case SGI_TO_LIST:
		gic_send_sgi_list(sgi, cpu_mask);
		break;
	default:
		pr_error("Sgi mode not supported\n");
		break;
	}
}

void gic_unmask_irq(struct vmm_irq *vmm_irq)
{
	uint32_t irq = vmm_irq->hno;
	uint32_t mask = 1 << (irq % 32);

	spin_lock(&gic_lock);

	if (irq < GICV3_NR_LOCAL_IRQS) {
		iowrite32(gicr_sgi_base() + GICR_ISENABLER + (irq / 32) * 4, mask);
		gic_gicr_wait_for_rwp();
	} else {
		iowrite32(gicd_base + GICD_ISENABLER + (irq / 32) * 4, mask);
		gic_gicd_wait_for_rwp();
	}
	vmm_irq->flags &= ~IRQ_STATUS_MASK;
	vmm_irq->flags |= IRQ_STATUS_UNMASKED;
	spin_unlock(&gic_lock);
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

uint32_t gic_get_line_num(void)
{
	uint32_t type;

	type = ioread32(gicd_base + GICD_TYPER);

	return (32 * ((type & 0x1f)));
}

static int gic_gicc_init(void)
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

static int gic_hyp_init(void)
{
	uint64_t value;

	write_sysreg32(GICH_VMCR_VEOIM | GICH_VMCR_VENG1, ICH_VMCR_EL2);
	write_sysreg32(GICH_HCR_EN, ICH_HCR_EL2);

	value = HCR_EL2_HVC | HCR_EL2_TWI | HCR_EL2_TWE | \
		HCR_EL2_TIDCP | HCR_EL2_IMO | HCR_EL2_FMO | \
		HCR_EL2_AMO | HCR_EL2_RW | HCR_EL2_VM;


	write_sysreg64(value, HCR_EL2);
	isb();
	return 0;
}

static int gic_gicr_init(void)
{
	int i;
	uint64_t pr;
	uint64_t reg_value;

	gic_wakeup_gicr();

	/* set the priority on PPI and SGI */
	pr = (0x90 << 24) | (0x90 << 16) | (0x90 << 8) | 0x90;
	for (i = 0; i < GICV3_NR_SGI; i += 4)
		iowrite32(gicr_sgi_base() + GICR_IPRIORITYR0 + (i / 4) * 4, pr);

	pr = (0xa0 << 24) | (0xa0 << 16) | (0xa0 << 8) | 0xa0;
	for (i = GICV3_NR_SGI; i < GICV3_NR_LOCAL_IRQS; i += 4)
		iowrite32(gicr_sgi_base() + GICR_IPRIORITYR0 + (i / 4) * 4, pr);

	/* disable all PPI and enable all SGI */
	iowrite32(gicr_sgi_base() + GICR_ICENABLER, 0xffff0000);
	iowrite32(gicr_sgi_base() + GICR_ISENABLER, 0x0000ffff);

	/* configure SGI and PPI as non-secure Group-1 */
	iowrite32(gicr_sgi_base() + GICR_IGROUPR0, 0xffffffff);

	gic_gicr_wait_for_rwp();
	isb();

	return 0;
}

void gic_init_el3(void)
{
	int i;
	void *gicr_rd_base;
	void *gicr_sgi_base;
	int cpuid = get_cpu_id();
	uint32_t val, nr_lines;

	gicr_rd_base = __gicr_rd_base + (128 * 1024) * cpuid;
	gicr_sgi_base = gicr_rd_base + (64 * 1024);

	if (cpuid == 0) {
		val = ioread32(gicd_base + GICD_TYPER);
		nr_lines = 32 * ((val & 0x1f));
		iowrite32(gicd_base + GICD_CTLR, (1 << 4) | (1 << 5));
		gic_gicd_wait_for_rwp();
	} else {
		val = (1 << 4) | (1 << 5);
		while (ioread32(gicd_base + GICD_CTLR) & (val) != val);
	}

	/* wake up GIC-R */
	val = ioread32(gicr_rd_base + GICR_WAKER);
	val &= ~(GICR_WAKER_PROCESSOR_SLEEP);
	iowrite32(gicr_rd_base + GICR_WAKER, val);

	while ((ioread32(gicr_rd_base + GICR_WAKER)
			& GICR_WAKER_CHILDREN_ASLEEP) != 0);

	if (cpuid == 0) {
		/* configure SPIs as non-secure GROUP-1 */
		for (i = GICV3_NR_LOCAL_IRQS; i < nr_lines; i += 32)
			iowrite32(gicd_base + GICD_IGROUPR + (i / 32) *4, 0xffffffff);
	}

	/* configure SGI and PPI as non-secure Group-1 */
	iowrite32(gicr_sgi_base + GICR_IGROUPR0, 0xffffffff);
}

void gic_init(void)
{
	int i;
	uint32_t type;
	uint32_t nr_lines;
	void *rbase;
	uint64_t pr;

	for (i = 0; i < CONFIG_NUM_OF_CPUS; i++) {
		rbase = __gicr_rd_base + (128 * 1024) * i;
		get_per_cpu(gicr_rd_base, i) = rbase;
		get_per_cpu(gicr_sgi_base, i) = rbase + (64 * 1024);
	}

	spin_lock_init(&gic_lock);

	spin_lock(&gic_lock);

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
		pr = ioread32(gicd_base + GICD_IPRIORITYR + (i / 4) * 4);
	}

	/* disable all global interrupt */
	for (i = GICV3_NR_LOCAL_IRQS; i < nr_lines; i += 32)
		iowrite32(gicd_base + GICD_ICENABLER + (i / 32) *4, 0xffffffff);

	/* configure SPIs as non-secure GROUP-1 */
	for (i = GICV3_NR_LOCAL_IRQS; i < nr_lines; i += 32)
		iowrite32(gicd_base + GICD_IGROUPR + (i / 32) *4, 0xffffffff);

	gic_gicd_wait_for_rwp();

	/* enable the gicd */
	iowrite32(gicd_base + GICD_CTLR, 1 | GICD_CTLR_ENABLE_GRP1 |
			GICD_CTLR_ENABLE_GRP1A | GICD_CTLR_ARE_NS);
	isb();

	gic_gicr_init();
	gic_gicc_init();
	gic_hyp_init();

	spin_unlock(&gic_lock);
}

void gic_secondary_init(void)
{
	spin_lock(&gic_lock);

	gic_gicr_init();
	gic_gicc_init();
	gic_hyp_init();

	spin_unlock(&gic_lock);
}
