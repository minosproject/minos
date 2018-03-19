#include <mvisor/types.h>
#include <mvisor/io.h>
#include <mvisor/percpu.h>
#include <mvisor/spinlock.h>
#include <mvisor/irq.h>
#include <mvisor/print.h>
#include <asm/gicv3.h>
#include <mvisor/errno.h>
#include <mvisor/module.h>

spinlock_t gicv3_lock;
static void *gicd_base = (void *)0x2f000000;
static void * __gicr_rd_base = (void *)0x2f100000;

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

void gicv3_mask_irq(struct vmm_irq *vmm_irq)
{
	uint32_t irq = vmm_irq->hno;
	uint32_t mask = 1 << (irq % 32);

	spin_lock(&gicv3_lock);
	if (irq < GICV3_NR_LOCAL_IRQS) {
		iowrite32(gicr_sgi_base() + GICR_ICENABLER + (irq / 32) * 4, mask);
		gicv3_gicr_wait_for_rwp();
	} else {
		iowrite32(gicd_base + GICD_ICENABLER + (irq / 32) * 4, mask);
		gicv3_gicd_wait_for_rwp();
	}

	vmm_irq->flags &= ~IRQ_STATUS_MASK;
	vmm_irq->flags |= IRQ_STATUS_MASKED;
	spin_unlock(&gicv3_lock);
}

void gicv3_eoi_irq(struct vmm_irq *vmm_irq)
{
	uint32_t irq = vmm_irq->hno;

	write_sysreg32(irq, ICC_EOIR1_EL1);
	isb();
}

void gicv3_dir_irq(struct vmm_irq *vmm_irq)
{
	uint32_t irq = vmm_irq->hno;

	write_sysreg32(irq, ICC_DIR_EL1);
	isb();
}

uint32_t gicv3_read_irq(void)
{
	uint32_t irq;

	irq = read_sysreg32(ICC_IAR1_EL1);
	dsbsy();
	return irq;
}

int gicv3_set_irq_type(struct vmm_irq *vmm_irq, uint32_t type)
{
	void *base;
	uint32_t irq = vmm_irq->hno;
	uint32_t cfg, actual, edgebit;

	spin_lock(&gicv3_lock);

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

	spin_unlock(&gicv3_lock);
}

int gicv3_set_irq_priority(struct vmm_irq *vmm_irq, uint32_t pr)
{
	uint32_t irq = vmm_irq->hno;

	spin_lock(&gicv3_lock);

	if (irq < GICV3_NR_LOCAL_IRQS)
		iowrite32(gicr_sgi_base() + GICR_IPRIORITYR0 + irq, pr);
	else
		iowrite32(gicd_base + GICD_IPRIORITYR + irq, pr);

	spin_unlock(&gicv3_lock);
}

int gicv3_set_irq_affinity(struct vmm_irq *vmm_irq, cpumask_t *dest)
{

}

static void gicv3_send_sgi_list(uint32_t sgi, cpumask_t *mask)
{
	int i;
	uint64_t list = 0;
	uint64_t val;

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		//if ((1 << i) & (*mask))
		//	list |= (1 << i);
	}

	/*
	 * TBD: now only support one cluster
	 */
	val = list | (0ul << 16) | (0ul << 32) |
		(0ul << 48) | (sgi << 24);
	write_sysreg64(val, ICC_SGI1R_EL1);
	isb();
}

uint64_t gicv3_read_lr(int lr)
{
	switch ( lr ) {
	case 0:
		return READ_SYSREG(ICH_LR0_EL2);
	case 1:
		return READ_SYSREG(ICH_LR1_EL2);
	case 2:
		return READ_SYSREG(ICH_LR2_EL2);
	case 3:
		return READ_SYSREG(ICH_LR3_EL2);
	case 4:
		return READ_SYSREG(ICH_LR4_EL2);
	case 5:
		return READ_SYSREG(ICH_LR5_EL2);
	case 6:
		return READ_SYSREG(ICH_LR6_EL2);
	case 7:
		return READ_SYSREG(ICH_LR7_EL2);
	case 8:
		return READ_SYSREG(ICH_LR8_EL2);
	case 9:
		return READ_SYSREG(ICH_LR9_EL2);
	case 10:
		return READ_SYSREG(ICH_LR10_EL2);
	case 11:
		return READ_SYSREG(ICH_LR11_EL2);
	case 12:
		return READ_SYSREG(ICH_LR12_EL2);
	case 13:
		return READ_SYSREG(ICH_LR13_EL2);
	case 14:
		return READ_SYSREG(ICH_LR14_EL2);
	case 15:
		return READ_SYSREG(ICH_LR15_EL2);
	default:
		panic("Invaild LR list\n");
}

static void gicv3_write_lr(int lr, uint64_t val)
{
	switch ( lr )
	{
	case 0:
		WRITE_SYSREG(val, ICH_LR0_EL2);
		break;
	case 1:
		WRITE_SYSREG(val, ICH_LR1_EL2);
		break;
	case 2:
		WRITE_SYSREG(val, ICH_LR2_EL2);
		break;
	case 3:
		WRITE_SYSREG(val, ICH_LR3_EL2);
		break;
	case 4:
		WRITE_SYSREG(val, ICH_LR4_EL2);
		break;
	case 5:
		WRITE_SYSREG(val, ICH_LR5_EL2);
		break;
	case 6:
		WRITE_SYSREG(val, ICH_LR6_EL2);
		break;
	case 7:
		WRITE_SYSREG(val, ICH_LR7_EL2);
		break;
	case 8:
		WRITE_SYSREG(val, ICH_LR8_EL2);
		break;
	case 9:
		WRITE_SYSREG(val, ICH_LR9_EL2);
		break;
	case 10:
		WRITE_SYSREG(val, ICH_LR10_EL2);
		break;
	case 11:
		WRITE_SYSREG(val, ICH_LR11_EL2);
		break;
	case 12:
		WRITE_SYSREG(val, ICH_LR12_EL2);
		break;
	case 13:
		WRITE_SYSREG(val, ICH_LR13_EL2);
		break;
	case 14:
		WRITE_SYSREG(val, ICH_LR14_EL2);
		break;
	case 15:
		WRITE_SYSREG(val, ICH_LR15_EL2);
		break;
	default:
		return;
	}

	isb();
}

static int send_virtual_irq(void *c, uint32_t v, uint32_t h)
{
	int i, empty = 0xff;
	uint64_t *lr_base;
	uint64_t value;
	struct gic_lr *lr;

	lr = (struct gic_lr *)&value;

	if (c)
		lr_base = (uint64_t *)c;
	/*
	 * first find one empty list lr register
	 */
	for(i = 0; i < gicv3_nr_lr; i++) {
		if (c)
			value = lr_base[i];
		else
			value = gicv3_read_lr(i);

		if (lr->state == 0) {
			empty = i;
		}

		if (lr->p_intid == h)
			return 0;
	}

	lr->v_intid = v;
	lr->p_intid = h;
	lr->priority = 0;
	lr->group = 1;
	lr->hw = h;

	if (empty) {
		if (c)
			lr_base[empty] = value;
		else
			gicv3_write_lr(empty, value);
	}

out:
	return -EAGAIN;
}

void gicv3_send_sgi(struct vmm_irq *data, enum sgi_mode mode, cpumask_t *cpu)
{
	cpumask_t cpus_mask;
	uint32_t sgi = data->hno;

	if (sgi > 15)
		return;

	switch (mode) {
	case SGI_TO_OTHERS:
		write_sysreg64(ICH_SGI_TARGET_OTHERS << ICH_SGI_IRQMODE_SHIFT |
				(uint64_t)sgi << ICH_SGI_IRQ_SHIFT, ICC_SGI1R_EL1);
		isb();
		break;
	case SGI_TO_SELF:
		//cpus_mask |= (1 << get_cpu_id());
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

void gicv3_unmask_irq(struct vmm_irq *vmm_irq)
{
	uint32_t irq = vmm_irq->hno;
	uint32_t mask = 1 << (irq % 32);

	spin_lock(&gicv3_lock);

	if (irq < GICV3_NR_LOCAL_IRQS) {
		iowrite32(gicr_sgi_base() + GICR_ISENABLER + (irq / 32) * 4, mask);
		gicv3_gicr_wait_for_rwp();
	} else {
		iowrite32(gicd_base + GICD_ISENABLER + (irq / 32) * 4, mask);
		gicv3_gicd_wait_for_rwp();
	}
	vmm_irq->flags &= ~IRQ_STATUS_MASK;
	vmm_irq->flags |= IRQ_STATUS_UNMASKED;
	spin_unlock(&gicv3_lock);
}

static int gicv3_get_irq_type(uint32_t irq)
{
	return 0;
}

static int gicv3_handle_sgi_int(uint32_t irq, vcpu_t *vcpu)
{
	return 0;
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

int gicv3_get_virq_state(struct vcpu_irq *vcpu_irq, void *context)
{
	return 0;
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
	write_sysreg32(GICH_VMCR_VEOIM | GICH_VMCR_VENG1, ICH_VMCR_EL2);
	write_sysreg32(GICH_HCR_EN, ICH_HCR_EL2);

	/*
	 * set IMO and FMO let physic irq and fiq taken to
	 * EL2, without this irq and fiq will not send to
	 * the cpu
	 */
	//write_sysreg64(HCR_EL2_IMO | HCR_EL2_FMO, HCR_EL2);
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

static void gicv3_state_save(vcpu_t *vcpu, void *context)
{

}

static void gicv3_state_restore(vcpu_t *vcpu, void *context)
{

}

static void gicv3_state_init(vcpu_t *vcpu, void *context)
{
	struct gic_context *c = (struct gic_context *)context;


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
	vmm_alloc_irqs(0, 31, 1);

	/* alloc SPI irqs */
	vmm_alloc_irqs(32, nr_lines - 1, 0);

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
	.irq_set_type 		= gicv3_set_irq_type,
	.irq_set_affinity 	= gicv3_set_irq_affinity,
	.send_sgi		= gicv3_send_sgi,
	.get_pending_irq	= gicv3_read_irq,
	.get_irq_type		= gicv3_get_irq_type,
	.irq_set_priority	= gicv3_set_irq_priority,
	.get_virq_state		= gicv3_get_virq_state,
	.init			= gicv3_init,
	.secondary_init		= gicv3_secondary_init,
};

static int gicv3_module_init(struct vmm_module *module)
{
	uint32_t type, nr_lines;
	uint32_t value;

	type = ioread32(gicd_base + GICD_TYPER);
	nr_lines = 32 * ((type & 0x1f));

	gicv3_chip.irq_start = 0;
	gicv3_chip.irq_num = nr_lines;

	value = READ_SYSREG32(ICH_VTR_EL2);
	gicv3_nr_lr = (value & 0x3f) + 1;
	gicv3_nr_pr = ((value >> 29) & 0x7) + 1;

	if (!((gicv3_nr_pr > 4) && (gicv3_nr_pr < 8)))
		panic("GICv3: Invalid number of priority bits\n");

	module->context_size = sizeof(struct gic_context);
	module->pdata = (void *)&gicv3_chip;
	module->state_init = gicv3_state_init;
	module->state_save = gicv3_state_save;
	module->state_restore = gicv3_state_restore;

	return 0;
}

VMM_MODULE_DECLARE(gicv3, "gicv3",
	"irq_chip", (void *)gicv3_module_init);
