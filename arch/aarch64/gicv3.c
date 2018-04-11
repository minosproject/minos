#include <mvisor/types.h>
#include <mvisor/io.h>
#include <mvisor/percpu.h>
#include <mvisor/spinlock.h>
#include <mvisor/print.h>
#include <asm/gicv3.h>
#include <mvisor/errno.h>
#include <mvisor/module.h>
#include <mvisor/vcpu.h>
#include <mvisor/panic.h>
#include <asm/arch.h>
#include <mvisor/cpumask.h>

spinlock_t gicv3_lock;
static void *gicd_base = (void *)0x2f000000;
static void * __gicr_rd_base = (void *)0x2f100000;
static int gicv3_module_id = 0xffff;

static int gicv3_nr_lr = 0;
static int gicv3_nr_pr = 0;

DEFINE_PER_CPU(void *, gicr_rd_base);
DEFINE_PER_CPU(void *, gicr_sgi_base);

#define gicr_rd_base()	get_cpu_var(gicr_rd_base)
#define gicr_sgi_base()	get_cpu_var(gicr_sgi_base)

static void gicv3_gicd_wait_for_rwp(void)
{
	while (ioread32(gicd_base + GICD_CTLR) & (1 << 31));
}

static void gicv3_gicr_wait_for_rwp(void)
{
	while (ioread32(gicr_rd_base() + GICR_CTLR) & (1 << 3));
}

static void gicv3_mask_irq(uint32_t irq)
{
	uint32_t mask = 1 << (irq % 32);

	spin_lock(&gicv3_lock);
	if (irq < GICV3_NR_LOCAL_IRQS) {
		iowrite32(gicr_sgi_base() + GICR_ICENABLER + (irq / 32) * 4, mask);
		gicv3_gicr_wait_for_rwp();
	} else {
		iowrite32(gicd_base + GICD_ICENABLER + (irq / 32) * 4, mask);
		gicv3_gicd_wait_for_rwp();
	}
	spin_unlock(&gicv3_lock);
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
	uint32_t cfg, actual, edgebit;

	spin_lock(&gicv3_lock);

	if (irq >= GICV3_NR_LOCAL_IRQS)
		base = (void *)gicd_base + GICD_ICFGR + (irq / 16) * 4;
	else
		base = (void *)gicr_sgi_base() + GICR_ICFGR1;

	cfg = ioread32(base);
	edgebit = 2u << (2 * (irq % 16));
	if (type & IRQ_FLAG_TYPE_LEVEL_BOTH)
		cfg &= ~edgebit;
	else if (type & IRQ_FLAG_TYPE_EDGE_BOTH)
		cfg |= edgebit;

	iowrite32(base, cfg);

	spin_unlock(&gicv3_lock);
}

static int gicv3_set_irq_priority(uint32_t irq, uint32_t pr)
{
	spin_lock(&gicv3_lock);

	if (irq < GICV3_NR_LOCAL_IRQS)
		iowrite8(gicr_sgi_base() + GICR_IPRIORITYR0 + irq, pr);
	else
		iowrite8(gicd_base + GICD_IPRIORITYR + irq, pr);

	spin_unlock(&gicv3_lock);
}

static int gicv3_set_irq_affinity(uint32_t irq, uint32_t pcpu)
{
	uint64_t affinity;

	affinity = logic_cpu_to_irq_affinity(pcpu);
	affinity &= ~(1 << 31); //GICD_IROUTER_SPI_MODE_ANY

	spin_lock(&gicv3_lock);
	iowrite64(gicd_base + GICD_IROUTER + irq * 8, affinity);
	spin_unlock(&gicv3_lock);
}

static void gicv3_send_sgi_list(uint32_t sgi, cpumask_t *mask)
{
	int i;
	uint64_t list = 0;
	uint64_t val;
	int cpu;

	for_each_cpu(cpu, mask)
		list |= (1 << cpu);

	/*
	 * TBD: now only support one cluster
	 */
	val = list | (0ul << 16) | (0ul << 32) |
		(0ul << 48) | (sgi << 24);
	write_sysreg64(val, ICC_SGI1R_EL1);
	isb();
}

static uint64_t gicv3_read_lr(int lr)
{
	switch (lr) {
	case 0: return read_sysreg(ICH_LR0_EL2);
	case 1: return read_sysreg(ICH_LR1_EL2);
	case 2: return read_sysreg(ICH_LR2_EL2);
	case 3: return read_sysreg(ICH_LR3_EL2);
	case 4: return read_sysreg(ICH_LR4_EL2);
	case 5: return read_sysreg(ICH_LR5_EL2);
	case 6: return read_sysreg(ICH_LR6_EL2);
	case 7: return read_sysreg(ICH_LR7_EL2);
	case 8: return read_sysreg(ICH_LR8_EL2);
	case 9: return read_sysreg(ICH_LR9_EL2);
	case 10: return read_sysreg(ICH_LR10_EL2);
	case 11: return read_sysreg(ICH_LR11_EL2);
	case 12: return read_sysreg(ICH_LR12_EL2);
	case 13: return read_sysreg(ICH_LR13_EL2);
	case 14: return read_sysreg(ICH_LR14_EL2);
	case 15: return read_sysreg(ICH_LR15_EL2);
	default:
		 return 0;
	}
}

static void gicv3_write_lr(int lr, uint64_t val)
{
	switch ( lr )
	{
	case 0:
		write_sysreg(val, ICH_LR0_EL2);
		break;
	case 1:
		write_sysreg(val, ICH_LR1_EL2);
		break;
	case 2:
		write_sysreg(val, ICH_LR2_EL2);
		break;
	case 3:
		write_sysreg(val, ICH_LR3_EL2);
		break;
	case 4:
		write_sysreg(val, ICH_LR4_EL2);
		break;
	case 5:
		write_sysreg(val, ICH_LR5_EL2);
		break;
	case 6:
		write_sysreg(val, ICH_LR6_EL2);
		break;
	case 7:
		write_sysreg(val, ICH_LR7_EL2);
		break;
	case 8:
		write_sysreg(val, ICH_LR8_EL2);
		break;
	case 9:
		write_sysreg(val, ICH_LR9_EL2);
		break;
	case 10:
		write_sysreg(val, ICH_LR10_EL2);
		break;
	case 11:
		write_sysreg(val, ICH_LR11_EL2);
		break;
	case 12:
		write_sysreg(val, ICH_LR12_EL2);
		break;
	case 13:
		write_sysreg(val, ICH_LR13_EL2);
		break;
	case 14:
		write_sysreg(val, ICH_LR14_EL2);
		break;
	case 15:
		write_sysreg(val, ICH_LR15_EL2);
		break;
	default:
		return;
	}

	isb();
}

static int gicv3_send_virq(struct virq *virq)
{
	uint64_t value = 0;
	struct gic_lr *lr = (struct gic_lr *)&value;
	uint64_t *base;

	lr->v_intid = virq->v_intno;
	lr->p_intid = virq->h_intno;
	lr->priority = 0;
	lr->group = 1;
	lr->hw = virq->hw;
	lr->state = 1;

	gicv3_write_lr(virq->id, value);

	return 0;
}

static void gicv3_send_sgi(uint32_t sgi, enum sgi_mode mode, cpumask_t *cpu)
{
	cpumask_t cpus_mask;

	if (sgi > 15)
		return;

	cpumask_clear(&cpus_mask);

	switch (mode) {
	case SGI_TO_OTHERS:
		write_sysreg64(ICH_SGI_TARGET_OTHERS << ICH_SGI_IRQMODE_SHIFT |
				(uint64_t)sgi << ICH_SGI_IRQ_SHIFT, ICC_SGI1R_EL1);
		isb();
		break;
	case SGI_TO_SELF:
		cpumask_set_cpu(get_cpu_id(), &cpus_mask);
		gicv3_send_sgi_list(sgi, &cpus_mask);
		break;
	case SGI_TO_LIST:
		gicv3_send_sgi_list(sgi, cpu);
		break;
	default:
		pr_error("Sgi mode not supported\n");
		break;
	}
}

static void gicv3_unmask_irq(uint32_t irq)
{
	uint32_t mask = 1 << (irq % 32);

	spin_lock(&gicv3_lock);

	if (irq < GICV3_NR_LOCAL_IRQS) {
		iowrite32(gicr_sgi_base() + GICR_ISENABLER + (irq / 32) * 4, mask);
		gicv3_gicr_wait_for_rwp();
	} else {
		iowrite32(gicd_base + GICD_ISENABLER + (irq / 32) * 4, mask);
		gicv3_gicd_wait_for_rwp();
	}

	spin_unlock(&gicv3_lock);
}

static int gicv3_get_irq_type(uint32_t irq)
{
	if (irq <= 15)
		return IRQ_TYPE_SGI;
	else if ((irq > 15) && (irq < 32))
		return IRQ_TYPE_PPI;
	else if ((irq >= 32) && (irq < 1020))
		return IRQ_TYPE_SPI;
	else if ((irq >= 1020) && (irq < 1024))
		return IRQ_TYPE_SPECIAL;
	else
		return IRQ_TYPE_BAD;
}

static void gicv3_wakeup_gicr(void)
{
	uint32_t gicv3_waker_value;

	gicv3_waker_value = ioread32(gicr_rd_base() + GICR_WAKER);
	gicv3_waker_value &= ~(GICR_WAKER_PROCESSOR_SLEEP);
	iowrite32(gicr_rd_base() + GICR_WAKER, gicv3_waker_value);

	while ((ioread32(gicr_rd_base() + GICR_WAKER)
			& GICR_WAKER_CHILDREN_ASLEEP) != 0);
}

uint32_t gicv3_get_irq_num(void)
{
	uint32_t type;

	type = ioread32(gicd_base + GICD_TYPER);

	return (32 * ((type & 0x1f)));
}

int gicv3_get_virq_state(struct virq *virq)
{
	uint64_t value;

	value = gicv3_read_lr(virq->id);
	value = (value & 0xc000000000000000) >> 62;

	return ((int)value);
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

	/*
	 * set IMO and FMO let physic irq and fiq taken to
	 * EL2, without this irq and fiq will not send to
	 * the cpu
	 */
	write_sysreg64(HCR_EL2_IMO | HCR_EL2_FMO, HCR_EL2);

	isb();
	return 0;
}

static int gicv3_gicr_init(void)
{
	int i;
	uint64_t pr;
	uint64_t reg_value;

	gicv3_wakeup_gicr();

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

	gicv3_gicr_wait_for_rwp();
	isb();

	return 0;
}

static void gicv3_save_lrs(struct gic_context *c, uint32_t count)
{
	if (count > 16)
		panic("Unsupport LR count\n");

	switch (count) {
	case 16:
		c->ich_lr15_el2 = read_sysreg(ICH_LR15_EL2);
	case 15:
		c->ich_lr14_el2 = read_sysreg(ICH_LR14_EL2);
	case 14:
		c->ich_lr13_el2 = read_sysreg(ICH_LR13_EL2);
	case 13:
		c->ich_lr12_el2 = read_sysreg(ICH_LR12_EL2);
	case 12:
		c->ich_lr11_el2 = read_sysreg(ICH_LR11_EL2);
	case 11:
		c->ich_lr10_el2 = read_sysreg(ICH_LR10_EL2);
	case 10:
		c->ich_lr9_el2 = read_sysreg(ICH_LR9_EL2);
	case 9:
		c->ich_lr8_el2 = read_sysreg(ICH_LR8_EL2);
	case 8:
		c->ich_lr7_el2 = read_sysreg(ICH_LR7_EL2);
	case 7:
		c->ich_lr6_el2 = read_sysreg(ICH_LR6_EL2);
	case 6:
		c->ich_lr5_el2 = read_sysreg(ICH_LR5_EL2);
	case 5:
		c->ich_lr4_el2 = read_sysreg(ICH_LR4_EL2);
	case 4:
		c->ich_lr3_el2 = read_sysreg(ICH_LR3_EL2);
	case 3:
		c->ich_lr2_el2 = read_sysreg(ICH_LR2_EL2);
	case 2:
		c->ich_lr1_el2 = read_sysreg(ICH_LR1_EL2);
	case 1:
		c->ich_lr0_el2 = read_sysreg(ICH_LR0_EL2);
		break;
	default:
		break;
	}
}

static void gicv3_save_aprn(struct gic_context *c, uint32_t count)
{
	switch (count) {
	case 7:
		c->ich_ap0r2_el2 = read_sysreg32(ICH_AP0R2_EL2);
		c->ich_ap1r2_el2 = read_sysreg32(ICH_AP1R2_EL2);
	case 6:
		c->ich_ap0r1_el2 = read_sysreg32(ICH_AP0R1_EL2);
		c->ich_ap1r1_el2 = read_sysreg32(ICH_AP1R1_EL2);
	case 5:
		c->ich_ap0r0_el2 = read_sysreg32(ICH_AP0R0_EL2);
		c->ich_ap1r0_el2 = read_sysreg32(ICH_AP1R0_EL2);
		break;
	default:
		panic("Unsupport aprn count\n");
	}
}

static void gicv3_state_save(struct vcpu *vcpu, void *context)
{
	struct gic_context *c = (struct gic_context *)context;

	gicv3_save_lrs(c, gicv3_nr_lr);
	gicv3_save_aprn(c, gicv3_nr_pr);
	c->icc_sre_el1 = read_sysreg32(ICC_SRE_EL1);
	c->ich_vmcr_el2 = read_sysreg32(ICH_VMCR_EL2);
	c->ich_hcr_el2 = read_sysreg32(ICH_HCR_EL2);
}

static void gicv3_restore_aprn(struct gic_context *c, uint32_t count)
{
	switch (count) {
	case 7:
		write_sysreg32(c->ich_ap0r2_el2, ICH_AP0R2_EL2);
		write_sysreg32(c->ich_ap1r2_el2, ICH_AP1R2_EL2);
	case 6:
		write_sysreg32(c->ich_ap0r1_el2, ICH_AP0R1_EL2);
		write_sysreg32(c->ich_ap1r1_el2, ICH_AP1R1_EL2);
	case 5:
		write_sysreg32(c->ich_ap0r0_el2, ICH_AP0R0_EL2);
		write_sysreg32(c->ich_ap1r0_el2, ICH_AP1R0_EL2);
		break;
	default:
		panic("Unsupport aprn count");
	}
}

static void gicv3_restore_lrs(struct gic_context *c, uint32_t count)
{
	if (count > 16)
		panic("Unsupport LR count");

	switch (count) {
	case 16:
		write_sysreg(c->ich_lr15_el2, ICH_LR15_EL2);
	case 15:
		write_sysreg(c->ich_lr14_el2, ICH_LR14_EL2);
	case 14:
		write_sysreg(c->ich_lr13_el2, ICH_LR13_EL2);
	case 13:
		write_sysreg(c->ich_lr12_el2, ICH_LR12_EL2);
	case 12:
		write_sysreg(c->ich_lr11_el2, ICH_LR11_EL2);
	case 11:
		write_sysreg(c->ich_lr10_el2, ICH_LR10_EL2);
	case 10:
		write_sysreg(c->ich_lr9_el2, ICH_LR9_EL2);
	case 9:
		write_sysreg(c->ich_lr8_el2, ICH_LR8_EL2);
	case 8:
		write_sysreg(c->ich_lr7_el2, ICH_LR7_EL2);
	case 7:
		write_sysreg(c->ich_lr6_el2, ICH_LR6_EL2);
	case 6:
		write_sysreg(c->ich_lr5_el2, ICH_LR5_EL2);
	case 5:
		write_sysreg(c->ich_lr4_el2, ICH_LR4_EL2);
	case 4:
		write_sysreg(c->ich_lr3_el2, ICH_LR3_EL2);
	case 3:
		write_sysreg(c->ich_lr2_el2, ICH_LR2_EL2);
	case 2:
		write_sysreg(c->ich_lr1_el2, ICH_LR1_EL2);
	case 1:
		write_sysreg(c->ich_lr0_el2, ICH_LR0_EL2);
		break;
	default:
		break;
	}
}

static void gicv3_state_restore(struct vcpu *vcpu, void *context)
{
	struct gic_context *c = (struct gic_context *)context;

	gicv3_restore_lrs(c, gicv3_nr_lr);
	gicv3_restore_aprn(c, gicv3_nr_pr);
	write_sysreg32(c->icc_sre_el1, ICC_SRE_EL1);
	write_sysreg32(c->ich_vmcr_el2, ICH_VMCR_EL2);
	write_sysreg32(c->ich_hcr_el2, ICH_HCR_EL2);
}

static void gicv3_state_init(struct vcpu *vcpu, void *context)
{
	struct gic_context *c = (struct gic_context *)context;

	memset((char *)c, 0, sizeof(struct gic_context));
	c->icc_sre_el1 = 0x7;
	c->ich_vmcr_el2 = GICH_VMCR_VENG1 | (0xff << 24);
	c->ich_hcr_el2 = GICH_HCR_EN;
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
		gicv3_gicd_wait_for_rwp();
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

int gicv3_init(void)
{
	int i;
	uint32_t type;
	uint32_t nr_lines;
	void *rbase;
	uint64_t pr;

	pr_info("*** gicv3 init ***\n");

	spin_lock_init(&gicv3_lock);

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		rbase = __gicr_rd_base + (128 * 1024) * i;
		get_per_cpu(gicr_rd_base, i) = rbase;
		get_per_cpu(gicr_sgi_base, i) = rbase + (64 * 1024);
	}

	spin_lock(&gicv3_lock);

	/* disable gicd */
	iowrite32(gicd_base + GICD_CTLR, 0);

	type = ioread32(gicd_base + GICD_TYPER);
	nr_lines = 32 * ((type & 0x1f));

	/* alloc LOCAL_IRQS for each cpus */
	irq_add_local(0, 32);

	/* alloc SPI irqs */
	irq_add_spi(32, nr_lines);

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

	gicv3_gicd_wait_for_rwp();

	/* enable the gicd */
	iowrite32(gicd_base + GICD_CTLR, 1 | GICD_CTLR_ENABLE_GRP1 |
			GICD_CTLR_ENABLE_GRP1A | GICD_CTLR_ARE_NS);
	isb();

	gicv3_gicr_init();
	gicv3_gicc_init();
	gicv3_hyp_init();

	spin_unlock(&gicv3_lock);
}

int gicv3_secondary_init(void)
{
	spin_lock(&gicv3_lock);

	gicv3_gicr_init();
	gicv3_gicc_init();
	gicv3_hyp_init();

	spin_unlock(&gicv3_lock);
}

static struct irq_chip gicv3_chip = {
	.irq_mask 		= gicv3_mask_irq,
	.irq_unmask 		= gicv3_unmask_irq,
	.irq_eoi 		= gicv3_eoi_irq,
	.irq_dir		= gicv3_dir_irq,
	.irq_set_type 		= gicv3_set_irq_type,
	.irq_set_affinity 	= gicv3_set_irq_affinity,
	.send_sgi		= gicv3_send_sgi,
	.get_pending_irq	= gicv3_read_irq,
	.irq_set_priority	= gicv3_set_irq_priority,
	.get_virq_state		= gicv3_get_virq_state,
	.send_virq		= gicv3_send_virq,
	.init			= gicv3_init,
	.secondary_init		= gicv3_secondary_init,
};

static int gicv3_module_init(struct vmm_module *module)
{
	uint32_t type, nr_lines;
	uint32_t value;

	value = read_sysreg32(ICH_VTR_EL2);
	gicv3_nr_lr = (value & 0x3f) + 1;
	gicv3_nr_pr = ((value >> 29) & 0x7) + 1;

	if (!((gicv3_nr_pr > 4) && (gicv3_nr_pr < 8)))
		panic("GICv3: Invalid number of priority bits\n");

	module->context_size = sizeof(struct gic_context);
	module->pdata = (void *)&gicv3_chip;
	module->state_init = gicv3_state_init;
	module->state_save = gicv3_state_save;
	module->state_restore = gicv3_state_restore;

	gicv3_module_id = module->id;

	return 0;
}

VMM_MODULE_DECLARE(gicv3, "gicv3",
	"irq_chip", (void *)gicv3_module_init);
