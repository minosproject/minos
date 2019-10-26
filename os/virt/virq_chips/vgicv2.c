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
#include <asm/arch.h>
#include <minos/vmodule.h>
#include <minos/irq.h>
#include <asm/gicv2.h>
#include <asm/io.h>
#include <minos/vmodule.h>
#include <minos/cpumask.h>
#include <minos/irq.h>
#include <minos/sched.h>
#include <virt/virq.h>
#include <virt/vdev.h>
#include <virt/resource.h>
#include <virt/virq_chip.h>
#include "vgic.h"
#include <minos/of.h>

struct vgicv2_dev {
	struct vdev vdev;
	uint32_t gicd_ctlr;
	uint32_t gicd_typer;
	uint32_t gicd_iidr;
	unsigned long gicd_base;
	unsigned long gicc_base;
	unsigned long gicc_size;
	uint8_t gic_cpu_id[8];
};

struct vgicv2_info {
	unsigned long gicd_base;
	unsigned long gicd_size;
	unsigned long gicc_base;
	unsigned long gicc_size;
	unsigned long gich_base;
	unsigned long gich_size;
	unsigned long gicv_base;
	unsigned long gicv_size;
};

struct vgicc {
	struct vdev vdev;
	unsigned long gicc_base;
	uint32_t gicc_ctlr;
	uint32_t gicc_pmr;
	uint32_t gicc_bpr;
};

static int gicv2_nr_lrs;
static struct vgicv2_info vgicv2_info;

#define vdev_to_vgicv2(vdev) \
	(struct vgicv2_dev *)container_of(vdev, struct vgicv2_dev, vdev)

#define vdev_to_vgicc(vdev) \
	(struct vgicc *)container_of(vdev, struct vgicc, vdev);

extern int gic_xlate_irq(struct device_node *node,
		uint32_t *intspec, unsigned int initsize,
		uint32_t *hwirq, unsigned long *type);

static uint32_t vgicv2_get_virq_type(struct vcpu *vcpu, uint32_t offset)
{
	int i;
	int irq;
	uint32_t value = 0, tmp;

	offset = (offset - GICD_ICFGR) / 4;
	irq = 16 * offset;

	for (i = 0; i < 16; i++, irq++) {
		tmp = virq_get_type(vcpu, irq);
		value = value | (tmp << i * 2);
	}

	return value;
}

static void vgicv2_set_virq_type(struct vcpu *vcpu,
		uint32_t offset, uint32_t value)
{
	int i;
	int irq;

	offset = (offset - GICD_ICFGR) / 4;
	irq = 16 * offset;

	for (i = 0; i < 16; i++, irq++) {
		virq_set_type(vcpu, irq, value & 0x3);
		value = value >> 2;
	}
}

static uint32_t vgicv2_get_virq_affinity(struct vcpu *vcpu,
		unsigned long offset)
{
	int i;
	int irq;
	uint32_t value = 0, t;

	offset = (offset - GICD_ITARGETSR) / 4;
	irq = 4 * offset;

	for (i = 0; i < 4; i++, irq++) {
		t = virq_get_affinity(vcpu, irq);
		value |= (1 << t) << (8 * i);
	}

	return value;
}

static uint32_t vgicv2_get_virq_pr(struct vcpu *vcpu,
		unsigned long offset)
{
	int i;
	uint32_t irq;
	uint32_t value = 0, t;

	offset = (offset - GICD_IPRIORITYR) / 4;
	irq = offset * 4;

	for (i = 0; i < 4; i++, irq++) {
		t = virq_get_pr(vcpu, irq);
		value |= t << (8 * i);
	}

	return value;
}

static uint32_t inline vgicv2_get_virq_state(struct vcpu *vcpu,
		unsigned long offset, unsigned long reg)
{
	int i;
	uint32_t irq;
	uint32_t value = 0, t;

	offset = (offset - reg) / 4;
	irq = offset * 32;

	for (i = 0; i < 32; i++, irq++) {
		t = virq_get_state(vcpu, irq);
		value |= t << i;
	}

	return value;
}

static uint32_t vgicv2_get_virq_mask(struct vcpu *vcpu,
		unsigned long offset)
{
	return vgicv2_get_virq_state(vcpu, offset, GICD_ICENABLER);
}

static uint32_t vgicv2_get_virq_unmask(struct vcpu *vcpu,
		unsigned long offset)
{
	return vgicv2_get_virq_state(vcpu, offset, GICD_ISENABLER);
}

static int vgicv2_read(struct vcpu *vcpu, struct vgicv2_dev *gic,
		unsigned long offset, unsigned long *v)
{
	uint32_t tmp;
	uint32_t *value = (uint32_t *)v;

	/* to be done */
	switch (offset) {
	case GICD_CTLR:
		*value = !!gic->gicd_ctlr;
		break;
	case GICD_TYPER:
		*value = gic->gicd_typer;
		break;
	case GICD_IGROUPR...GICD_IGROUPRN:
		/* all group 1 */
		*value = 0xffffffff;
		break;
	case GICD_ISENABLER...GICD_ISENABLERN:
		*value = vgicv2_get_virq_unmask(vcpu, offset);
		break;
	case GICD_ICENABLER...GICD_ICENABLERN:
		*value = vgicv2_get_virq_mask(vcpu, offset);
		break;
	case GICD_ISPENDR...GICD_ISPENDRN:
		*value = 0;
		break;
	case GICD_ICPENDR...GICD_ICPENDRN:
		*value = 0;
		break;
	case GICD_ISACTIVER...GICD_ISACTIVERN:
		*value = 0;
		break;
	case GICD_ICACTIVER...GICD_ICACTIVERN:
		*value = 0;
		break;
	case GICD_IPRIORITYR...GICD_IPRIORITYRN:
		*value = vgicv2_get_virq_pr(vcpu, offset);
		break;
	case GICD_ITARGETSR...GICD_ITARGETSR7:
		tmp = 1 << get_vcpu_id(vcpu);
		*value = tmp;
		*value |= tmp << 8;
		*value |= tmp << 16;
		*value |= tmp << 24;
		break;
	case GICD_ITARGETSR8...GICD_ITARGETSRN:
		*value = vgicv2_get_virq_affinity(vcpu, offset);
		break;
	case GICD_ICFGR...GICD_ICFGRN:
		*value = vgicv2_get_virq_type(vcpu, offset);
		break;

	case GICD_ICPIDR2:
		*value = 0x2 << 4;
	}

	return 0;
}

void vgicv2_send_sgi(struct vcpu *vcpu, uint32_t sgi_value)
{
	int bit;
	sgi_mode_t mode;
	uint32_t sgi;
	cpumask_t cpumask;
	unsigned long list;
	struct vm *vm = vcpu->vm;
	struct vcpu *target;

	cpumask_clearall(&cpumask);
	list = (sgi_value >> 16) & 0xff;
	sgi = sgi_value & 0xf;
	mode = (sgi_value >> 24) & 0x3;
	if (mode == 0x3) {
		pr_warn("invalid sgi mode\n");
		return;
	}

	if (mode == SGI_TO_LIST) {
		for_each_set_bit(bit, &list, 8)
			cpumask_set_cpu(bit, &cpumask);
	} else if (mode == SGI_TO_OTHERS) {
		for (bit = 0; bit < vm->vcpu_nr; bit++) {
			if (bit == vcpu->vcpu_id)
				continue;
			cpumask_set_cpu(bit, &cpumask);
		}
	} else
		cpumask_set_cpu(smp_processor_id(), &cpumask);

	for_each_cpu(bit, &cpumask) {
		target = get_vcpu_in_vm(vm, bit);
		send_virq_to_vcpu(target, sgi);
	}
}

static int vgicv2_write(struct vcpu *vcpu, struct vgicv2_dev *gic,
		unsigned long offset, unsigned long *v)
{
	uint32_t x, y, bit, t;
	uint32_t value = *(uint32_t *)v;

	/* to be done */
	switch (offset) {
	case GICD_CTLR:
		gic->gicd_ctlr = value;
		break;
	case GICD_TYPER:
		break;
	case GICD_IGROUPR...GICD_IGROUPRN:
		break;
	case GICD_ISENABLER...GICD_ISENABLERN:
		x = (offset - GICD_ISENABLER) / 4;
		y = x * 32;
		for_each_set_bit(bit, v, 32)
			virq_enable(vcpu, y + bit);
		break;
	case GICD_ICENABLER...GICD_ICENABLERN:
		x = (offset - GICD_ICENABLER) / 4;
		y = x * 32;
		for_each_set_bit(bit, v, 32)
			virq_disable(vcpu, y + bit);
		break;
	case GICD_ISPENDR...GICD_ISPENDRN:
		break;
	case GICD_ICPENDR...GICD_ICPENDRN:
		break;
	case GICD_ISACTIVER...GICD_ISACTIVERN:
		break;
	case GICD_ICACTIVER...GICD_ICACTIVERN:
		break;
	case GICD_IPRIORITYR...GICD_IPRIORITYRN:
		t = value;
		x = (offset - GICD_IPRIORITYR) / 4;
		y = x * 4 - 1;
		bit = (t & 0x000000ff);
		virq_set_priority(vcpu, y + 1, bit);
		bit = (t & 0x0000ff00) >> 8;
		virq_set_priority(vcpu, y + 2, bit);
		bit = (t & 0x00ff0000) >> 16;
		virq_set_priority(vcpu, y + 3, bit);
		bit = (t & 0xff000000) >> 24;
		virq_set_priority(vcpu, y + 4, bit);
		break;
	case GICD_ITARGETSR8...GICD_ITARGETSRN:
		/* to be done */
		break;
	case GICD_ICFGR...GICD_ICFGRN:
		vgicv2_set_virq_type(vcpu, offset, value);
		break;

	case GICD_SGIR:
		vgicv2_send_sgi(vcpu, value);
		break;
	}

	return 0;
}

static int vgicv2_mmio_handler(struct vdev *vdev, gp_regs *regs,
		int read, unsigned long address, unsigned long *value)
{
	unsigned long offset;
	struct vcpu *vcpu = get_current_vcpu();
	struct vgicv2_dev *gic = vdev_to_vgicv2(vdev);

	offset = address - gic->gicd_base;
	if (read)
		return vgicv2_read(vcpu, gic, offset, value);
	else
		return vgicv2_write(vcpu, gic, offset, value);
}

static int vgicv2_mmio_read(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *read_value)
{
	return vgicv2_mmio_handler(vdev, regs, 1, address, read_value);
}

static int vgicv2_mmio_write(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *write_value)
{
	return vgicv2_mmio_handler(vdev, regs, 0, address, write_value);
}

static void vgicv2_reset(struct vdev *vdev)
{
	pr_info("vgicv2 device reset\n");
}

static void vgicv2_deinit(struct vdev *vdev)
{
	struct vgicv2_dev *dev = vdev_to_vgicv2(vdev);

	if (!dev)
		return;

	vdev_release(&dev->vdev);
	free(dev);
}

static int vgicc_read(struct vdev *vdev, gp_regs *reg,
		unsigned long address, unsigned long *value)
{
	struct vgicc *vgicc = vdev_to_vgicc(vdev);
	unsigned long offset = address - vgicc->gicc_base;

	switch (offset) {
	case GICC_CTLR:
		*value = vgicc->gicc_ctlr;
		break;
	case GICC_PMR:
		*value = vgicc->gicc_pmr;
		break;
	case GICC_BPR:
		*value = vgicc->gicc_bpr;
		break;
	case GICC_IAR:
		/* get the pending irq number */
		*value = get_pending_virq(get_current_vcpu());
		break;
	case GICC_RPR:
		/* TBD - now fix to 0xa0 */
		*value = 0xa0;
		break;
	case GICC_HPPIR:
		/* TBD - now fix to 0xa0 */
		*value = 0xa0;
		break;
	case GICC_IIDR:
		*value = 0x43b | (0x2 << 16);
		break;
	}

	return 0;
}

static int vgicc_write(struct vdev *vdev, gp_regs *reg,
		unsigned long address, unsigned long *value)
{
	struct vgicc *vgicc = vdev_to_vgicc(vdev);
	unsigned long offset = address - vgicc->gicc_base;

	switch (offset) {
	case GICC_CTLR:
		vgicc->gicc_ctlr = *value;
		break;
	case GICC_PMR:
		vgicc->gicc_pmr = *value;
		break;
	case GICC_BPR:
		vgicc->gicc_bpr = *value;
		break;
	case GICC_EOIR:
		clear_pending_virq(get_current_vcpu(), *value);
		break;
	case GICC_DIR:
		/* if the virq is hw to deactive it TBD */
		break;
	}

	return 0;
}

static void vgicc_reset(struct vdev *vdev)
{
}

static void vgicc_deinit(struct vdev *vdev)
{
	vdev_release(vdev);
	free(vdev);
}

static int vgicv2_create_vgicc(struct vm *vm,
		unsigned long base, size_t size)
{
	struct vgicc *vgicc;

	vgicc = zalloc(sizeof(struct vdev));
	if (!vgicc) {
		pr_err("no memory for vgicv2 vgicc\n");
		return -ENOMEM;
	}

	host_vdev_init(vm, &vgicc->vdev, base, size);
	vdev_set_name(&vgicc->vdev, "vgicv2_vgicc");
	vgicc->gicc_base = base;
	vgicc->vdev.read = vgicc_read;
	vgicc->vdev.write = vgicc_write;
	vgicc->vdev.reset = vgicc_reset;
	vgicc->vdev.deinit = vgicc_deinit;

	return 0;
}

static inline void writel_gich(uint32_t val, unsigned int offset)
{
	writel_relaxed(val, (void *)vgicv2_info.gich_base + offset);
}

static inline uint32_t readl_gich(int unsigned offset)
{
	return readl_relaxed((void *)vgicv2_info.gich_base + offset);
}

int gicv2_get_virq_state(struct vcpu *vcpu, struct virq_desc *virq)
{
	uint32_t value;

	if (virq->id >= gicv2_nr_lrs)
		return 0;

	value = readl_gich(GICH_LR + virq->id * 4);
	isb();
	value = (value >> 28) & 0x3;

	return value;
}

static int gicv2_send_virq(struct vcpu *vcpu, struct virq_desc *virq)
{
	uint32_t val;
	uint32_t pid = 0;
	struct gich_lr *gich_lr;

	if (virq->id >= gicv2_nr_lrs) {
		pr_err("invalid virq %d\n", virq->id);
		return -EINVAL;
	}

	if (virq_is_hw(virq))
		pid = virq->hno;
	else {
		if (virq->vno < 16)
			pid = virq->src;
	}

	gich_lr = (struct gich_lr *)&val;
	gich_lr->vid = virq->vno;
	gich_lr->pid = pid;
	gich_lr->pr = virq->pr;
	gich_lr->grp1 = 0;
	gich_lr->state = 1;
	gich_lr->hw = !!virq_is_hw(virq);

	writel_gich(val, GICH_LR + virq->id * 4);
	isb();

	return 0;
}

static int gicv2_update_virq(struct vcpu *vcpu,
		struct virq_desc *desc, int action)
{
	if (!desc || desc->id >= gicv2_nr_lrs)
		return -EINVAL;

	switch (action) {
	case VIRQ_ACTION_REMOVE:
		if (virq_is_hw(desc))
			irq_clear_pending(desc->hno);

	case VIRQ_ACTION_CLEAR:
		writel_gich(0, GICH_LR + desc->id * 4);
		isb();
		break;
	}

	return 0;
}

static int vgicv2_init_virqchip(struct virq_chip *vc,
		void *dev, unsigned long flags)
{
	if (flags & VIRQCHIP_F_HW_VIRT) {
		vc->nr_lrs = gicv2_nr_lrs;
		vc->exit_from_guest = vgic_irq_exit_from_guest;
		vc->enter_to_guest = vgic_irq_enter_to_guest;
		vc->send_virq = gicv2_send_virq;
		vc->update_virq = gicv2_update_virq;
		vc->get_virq_state = gicv2_get_virq_state;
	}

	vc->xlate = gic_xlate_irq;
	vc->vm0_virq_data = gic_vm0_virq_data;
	vc->flags = flags;
	vc->inc_pdata = dev;

	return 0;
}

static struct virq_chip *vgicv2_virqchip_init(struct vm *vm,
		struct device_node *node)
{
	int ret, flags = 0;
	struct vgicv2_dev *dev;
	struct virq_chip *vc;
	uint64_t gicd_base, gicd_size;
	uint64_t gicc_base, gicc_size;

	pr_info("create vgicv2 for vm-%d\n", vm->vmid);

	ret = translate_device_address_index(node, &gicd_base,
			&gicd_size, 0);
	ret += translate_device_address_index(node, &gicc_base,
			&gicc_size, 1);
	if (ret || (gicd_size == 0) || (gicc_size == 0))
		return NULL;

	pr_info("vgicv2 address 0x%x 0x%x 0x%x 0x%x\n",
				gicd_base, gicd_size,
				gicc_base, gicc_size);

	dev = zalloc(sizeof(struct vgicv2_dev));
	if (!dev)
		return NULL;

	dev->gicd_base = gicd_base;
	host_vdev_init(vm, &dev->vdev, gicd_base, gicd_size);
	vdev_set_name(&dev->vdev, "vgicv2");

	dev->gicd_typer = vm->vcpu_nr << 5;
	dev->gicd_typer |= (vm->vspi_nr >> 5) - 1;

	dev->gicd_iidr = 0x0;

	dev->vdev.read = vgicv2_mmio_read;
	dev->vdev.write = vgicv2_mmio_write;
	dev->vdev.deinit = vgicv2_deinit;
	dev->vdev.reset = vgicv2_reset;

	/*
	 * if the gicv base is set indicate that
	 * platform has a hardware gicv2, otherwise
	 * we need to emulated the trap.
	 */
	if (vgicv2_info.gicv_base != 0) {
		flags |= VIRQCHIP_F_HW_VIRT;
		pr_info("map gicc 0x%x to gicv 0x%x size 0x%x\n",
				gicc_base, vgicv2_info.gicv_base, gicc_size);

		create_guest_mapping(&vm->mm, gicc_base,
				vgicv2_info.gicv_base, gicc_size, VM_IO);
	} else {
		vgicv2_create_vgicc(vm, gicc_base, gicc_size);
	}

	/* create the virqchip and init it */
	vc = alloc_virq_chip();
	if (!vc)
		return NULL;

	vgicv2_init_virqchip(vc, dev, flags);

	return vc;
}
VIRQCHIP_DECLARE(gic400_virqchip, gicv2_match_table,
		vgicv2_virqchip_init);

static void gicv2_state_restore(struct task *task, void *context)
{
	int i;
	struct gicv2_context *c = (struct gicv2_context *)context;

	for (i = 0; i < gicv2_nr_lrs; i++)
		writel_gich(c->lr[i], GICH_LR + i * 4);

	writel_gich(c->apr, GICH_APR);
	writel_gich(c->vmcr, GICH_VMCR);
	writel_gich(c->hcr, GICH_HCR);
	isb();
}

static void gicv2_state_init(struct task *task, void *context)
{
	struct gicv2_context *c = (struct gicv2_context *)context;

	memset(c, 0, sizeof(*c));
	c->hcr = 1;
}

static void gicv2_state_save(struct task *task, void *context)
{
	int i;
	struct gicv2_context *c = (struct gicv2_context *)context;

	dsb();

	for (i = 0; i < gicv2_nr_lrs; i++)
		c->lr[i] = readl_gich(GICH_LR + i * 4);

	c->vmcr = readl_gich(GICH_VMCR);
	c->apr = readl_gich(GICH_APR);
	c->hcr = readl_gich(GICH_HCR);
	writel_gich(0, GICH_HCR);
	isb();
}

static void gicv2_state_resume(struct task *task, void *context)
{
	gicv2_state_init(task, context);
}

static int gicv2_vmodule_init(struct vmodule *vmodule)
{
	vmodule->context_size = sizeof(struct gicv2_context);
	vmodule->state_init = gicv2_state_init;
	vmodule->state_save = gicv2_state_save;
	vmodule->state_restore = gicv2_state_restore;
	vmodule->state_resume = gicv2_state_resume;

	return 0;
}

int vgicv2_init(uint64_t *data, int len)
{
	int i;
	uint32_t vtr;
	unsigned long *value = (unsigned long *)&vgicv2_info;

	for (i = 0; i < len; i++)
		value[i] = (unsigned long)data[i];

	for (i = 0; i < len; i++) {
		if (value[i] == 0)
			panic("invalid address of gicv2\n");
	}

	vtr = readl_relaxed((void *)vgicv2_info.gich_base + GICH_VTR);
	gicv2_nr_lrs = (vtr & 0x3f) + 1;
	pr_info("vgicv2 vtr 0x%x nr_lrs : 0x%d\n", vtr, gicv2_nr_lrs);

	register_task_vmodule("gicv2", gicv2_vmodule_init);

	return 0;
}
