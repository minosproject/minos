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
#include <minos/irq.h>
#include <device/gicv3.h>
#include <asm/io.h>
#include <virt/vmodule.h>
#include <minos/cpumask.h>
#include <minos/sched.h>
#include <virt/virq.h>
#include <virt/vdev.h>
#include <virt/resource.h>
#include <virt/virq_chip.h>
#include <minos/of.h>
#include "vgic.h"

struct vgic_gicd {
	uint32_t gicd_ctlr;
	uint32_t gicd_typer;
	uint32_t gicd_pidr2;
	spinlock_t gicd_lock;
	unsigned long base;
	unsigned long end;
};

struct vgic_gicr {
	uint32_t gicr_ctlr;
	uint32_t gicr_pidr2;
	uint64_t gicr_typer;
	uint32_t gicr_ispender;
	uint32_t gicr_enabler0;
	uint32_t vcpu_id;
	unsigned long rd_base;
	unsigned long sgi_base;
	unsigned long vlpi_base;
	struct list_head list;
	spinlock_t gicr_lock;
};

struct vgicv3_dev {
	struct vdev vdev;
	struct vgic_gicd gicd;
	struct vgic_gicr *gicr[NR_CPUS];
};

#define GIC_TYPE_GICD		(0x0)
#define GIC_TYPE_GICR_RD	(0x1)
#define GIC_TYPE_GICR_SGI	(0x2)
#define GIC_TYPE_GICR_VLPI	(0x3)
#define GIC_TYPE_INVAILD	(0xff)

#define vdev_to_vgic(vdev) \
	(struct vgicv3_dev *)container_of(vdev, struct vgicv3_dev, vdev)

struct vgicv3_info {
	unsigned long gicd_base;
	unsigned long gicd_size;
	unsigned long gicr_base;
	unsigned long gicr_size;
	unsigned long gicc_base;
	unsigned long gicc_size;
	unsigned long gich_base;
	unsigned long gich_size;
	unsigned long gicv_base;
	unsigned long gicv_size;
};

#define VGICV3_IDX_GICD 0x0
#define VGICV3_IDX_GICR 0x1
#define VGICV3_IDX_GICC 0x2
#define VGICV3_IDX_GICH 0x3
#define VGICV3_IDX_GICV 0x4

static int gicv3_nr_lr = 0;
static int gicv3_nr_pr = 0;
static struct vgicv3_info vgicv3_info;

extern int gic_xlate_irq(struct device_node *node,
		uint32_t *intspec, unsigned int initsize,
		uint32_t *hwirq, unsigned long *type);

static int vgicv3_send_sgi(struct vcpu *vcpu, unsigned long sgi_value)
{
	sgi_mode_t mode;
	uint32_t sgi;
	cpumask_t cpumask;
	unsigned long tmp, aff3, aff2, aff1;
	int bit, logic_cpu;
	struct vm *vm = vcpu->vm;
	struct vcpu *target;

	sgi = (sgi_value & (0xf << 24)) >> 24;
	if (sgi >= 16) {
		pr_err("vgic : sgi number is incorrect %d\n", sgi);
		return -EINVAL;
	}

	mode = sgi_value & (1UL << 40) ? SGI_TO_OTHERS : SGI_TO_LIST;
	cpumask_clearall(&cpumask);

	if (mode == SGI_TO_LIST) {
		tmp = sgi_value & 0xffff;
		aff3 = (sgi_value & (0xffUL << 48)) >> 48;
		aff2 = (sgi_value & (0xffUL << 32)) >> 32;
		aff1 = (sgi_value & (0xffUL << 16)) >> 16;
		for_each_set_bit(bit, &tmp, 16) {
			logic_cpu = affinity_to_logic_cpu(aff3, aff2, aff1, bit);
			cpumask_set_cpu(logic_cpu, &cpumask);
		}
	} else if (mode == SGI_TO_OTHERS) {
		for (bit = 0; bit < vm->vcpu_nr; bit++) {
			if (bit == vcpu->vcpu_id)
				continue;
			cpumask_set_cpu(bit, &cpumask);
		}
	} else
		cpumask_set_cpu(smp_processor_id(), &cpumask);

	/*
	 * here we update the gicr releated register
	 * for some other purpose use TBD
	 */

	for_each_cpu(bit, &cpumask) {
		target = get_vcpu_in_vm(vm, bit);
		send_virq_to_vcpu(target, sgi);
	}

	return 0;
}

static int offset_to_gicr_type(struct vgic_gicr *gicr, unsigned long *poffset)
{
	unsigned long offset = *poffset;

	if ((offset >= gicr->rd_base) &&
		(offset < (gicr->rd_base + SIZE_64K))) {
		*poffset = offset - gicr->rd_base;
		return GIC_TYPE_GICR_RD;
	}

	if ((offset >= gicr->sgi_base) &&
		(offset < (gicr->sgi_base + SIZE_64K))) {
		*poffset = offset - gicr->sgi_base;
		return GIC_TYPE_GICR_SGI;
	}
#if 0
	if ((address >= gicr->vlpi_base) &&
		(address < (gicr->vlpi_base + SIZE_64K))) {

		*offset = address - gicr->vlpi_base;
		return GIC_TYPE_GICR_VLPI;
	}
#endif
	return GIC_TYPE_INVAILD;
}

static uint32_t vgic_get_virq_type(struct vcpu *vcpu, uint32_t offset)
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

static void vgic_set_virq_type(struct vcpu *vcpu,
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

static int vgic_gicd_mmio_read(struct vcpu *vcpu,
			struct vgic_gicd *gicd,
			unsigned long offset,
			unsigned long *v)
{
	uint32_t *value = (uint32_t *)v;

	switch (offset) {
		case GICD_CTLR:
			*value = gicd->gicd_ctlr & ~(1 << 31);
			break;
		case GICD_TYPER:
			*value = gicd->gicd_typer;
			break;
		case GICD_STATUSR:
			*value = 0;
			break;
		case GICD_ISENABLER...GICD_ISENABLER_END:
			*value = 0;
			break;
		case GICD_ICENABLER...GICD_ICENABLER_END:
			*value = 0;
			break;
		case GICD_PIDR2:
			*value = gicd->gicd_pidr2;
			break;
		case GICD_ICFGR...GICD_ICFGR_END:
			*value = vgic_get_virq_type(vcpu, offset);
			break;
		default:
			*value = 0;
			break;
	}

	return 0;
}

static int vgic_gicd_mmio_write(struct vcpu *vcpu,
			struct vgic_gicd *gicd,
			unsigned long offset,
			unsigned long *value)
{
	uint32_t x, y, bit, t;

	spin_lock(&gicd->gicd_lock);

	switch (offset) {
	case GICD_CTLR:
		gicd->gicd_ctlr = *value;
		break;
	case GICD_TYPER:
		break;
	case GICD_STATUSR:
		break;
	case GICD_ISENABLER...GICD_ISENABLER_END:
		x = (offset - GICD_ISENABLER) / 4;
		y = x * 32;
		for_each_set_bit(bit, value, 32)
			virq_enable(vcpu, y + bit);
		break;
	case GICD_ICENABLER...GICD_ICENABLER_END:
		x = (offset - GICD_ICENABLER) / 4;
		y = x * 32;
		for_each_set_bit(bit, value, 32)
			virq_disable(vcpu, y + bit);
		break;
	case GICD_IPRIORITYR...GICD_IPRIORITYR_END:
		t = *value;
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
	case GICD_ICFGR...GICD_ICFGR_END:
		vgic_set_virq_type(vcpu, offset, *value);
		break;

	default:
		break;
	}

	spin_unlock(&gicd->gicd_lock);
	return 0;
}

static int vgic_gicd_mmio(struct vcpu *vcpu, struct vgic_gicd *gicd,
		int read, unsigned long offset, unsigned long *value)
{
	if (read)
		return vgic_gicd_mmio_read(vcpu, gicd, offset, value);
	else
		return vgic_gicd_mmio_write(vcpu, gicd, offset, value);
}

static int vgic_gicr_rd_mmio(struct vcpu *vcpu, struct vgic_gicr *gicr,
		int read, unsigned long offset, unsigned long *value)
{
	if (read) {
		switch (offset) {
		case GICR_PIDR2:
			*value = gicr->gicr_pidr2;
			break;
		case GICR_TYPER:
			*value = gicr->gicr_typer;
			break;
		case GICR_TYPER_HIGH:
			*value = gicr->gicr_typer >> 32;	/* for aarch32 assume 32bit read */
			break;
		default:
			*value = 0;
			break;
		}
	} else {

	}

	return 0;
}

static int vgic_gicr_sgi_mmio(struct vcpu *vcpu, struct vgic_gicr *gicr,
		int read, unsigned long offset, unsigned long *value)
{
	int bit;

	if (read) {
		switch (offset) {
		case GICR_CTLR:
			*value = gicr->gicr_ctlr & ~(1 << 31);
			break;
		case GICR_ISPENDR0:
			*value = gicr->gicr_ispender;
			break;
		case GICR_PIDR2:
			*value = 0x3 << 4;
			break;
		case GICR_ISENABLER:
		case GICR_ICENABLER:
			*value = gicr->gicr_enabler0;
			break;
		default:
			*value = 0;
			break;
		}
	} else {
		switch (offset) {
		case GICR_ICPENDR0:
			spin_lock(&gicr->gicr_lock);
			for_each_set_bit(bit, value, 32) {
				gicr->gicr_ispender &= ~BIT(bit);
				clear_pending_virq(vcpu, bit);
			}
			spin_unlock(&gicr->gicr_lock);
			break;
		case GICR_ISENABLER:
			spin_lock(&gicr->gicr_lock);
			for_each_set_bit(bit, value, 32) {
				if (!(gicr->gicr_enabler0 & BIT(bit))) {
					virq_enable(vcpu, bit);
					gicr->gicr_enabler0 |= BIT(bit);
				}
			}
			spin_unlock(&gicr->gicr_lock);
			break;
		case GICR_ICENABLER:
			spin_lock(&gicr->gicr_lock);
			for_each_set_bit(bit, value, 32) {
				if (gicr->gicr_enabler0 & BIT(bit)) {
					virq_disable(vcpu, bit);
					gicr->gicr_enabler0 &= ~BIT(bit);
				}
			}
			spin_unlock(&gicr->gicr_lock);
			break;
		}
	}

	return 0;
}

static int vgic_gicr_vlpi_mmio(struct vcpu *vcpu, struct vgic_gicr *gicr,
		int read, unsigned long offset, unsigned long *v)
{
	return 0;
}

static int vgic_check_gicr_access(struct vcpu *vcpu, struct vgic_gicr *gicr,
			int type, unsigned long offset)
{
	if (get_vcpu_id(vcpu) != gicr->vcpu_id) {
		if (type == GIC_TYPE_GICR_RD) {
			switch (offset) {
			case GICR_TYPER:
			case GICR_PIDR2:
			case GICR_TYPER_HIGH:		// for aarch32 gicv3
				return 1;
			default:
				return 0;
			}
		} else {
			return 0;
		}
	}

	return 1;
}

static int vgic_mmio_handler(struct vdev *vdev, gp_regs *regs,
		int read, int idx, unsigned long offset,
		unsigned long *value)
{
	int type = GIC_TYPE_INVAILD;
	struct vgic_gicd *gicd = NULL;
	struct vgic_gicr *gicr = NULL;
	struct vcpu *vcpu = get_current_vcpu();
	struct vgicv3_dev *gic = vdev_to_vgic(vdev);
	int i;

	gicr = gic->gicr[get_vcpu_id(vcpu)];
	gicd = &gic->gicd;

	if (idx == VGICV3_IDX_GICD) {
		type = GIC_TYPE_GICD;
	} else if (idx == VGICV3_IDX_GICR) {
		type = offset_to_gicr_type(gicr, &offset);
		if (type != GIC_TYPE_INVAILD)
			goto out;

		/* master vcpu may access other vcpu's gicr */
		for (i = 0; i < vcpu->vm->vcpu_nr; i++) {
			gicr = gic->gicr[i];
			type = offset_to_gicr_type(gicr, &offset);
			if (type != GIC_TYPE_INVAILD)
				goto out;
		}
	} else {
		pr_err("only support GICD and GICR emulation\n");
		return -EINVAL;
	}
out:
	if (type == GIC_TYPE_INVAILD) {
		pr_err("invaild gicr type and address\n");
		return -EINVAL;
	}

	if (type != GIC_TYPE_GICD) {
		if (!vgic_check_gicr_access(vcpu, gicr, type, offset))
			return -EACCES;
	}

	switch (type) {
	case GIC_TYPE_GICD:
		return vgic_gicd_mmio(vcpu, gicd, read, offset, value);
	case GIC_TYPE_GICR_RD:
		return vgic_gicr_rd_mmio(vcpu, gicr, read, offset, value);
	case GIC_TYPE_GICR_SGI:
		return vgic_gicr_sgi_mmio(vcpu, gicr, read, offset, value);
	case GIC_TYPE_GICR_VLPI:
		return vgic_gicr_vlpi_mmio(vcpu, gicr, read, offset, value);
	default:
		pr_err("unsupport gic type %d\n", type);
		return -EINVAL;
	}

	return 0;
}

static int vgic_mmio_read(struct vdev *vdev, gp_regs *regs, int idx,
		unsigned long address, unsigned long *read_value)
{
	return vgic_mmio_handler(vdev, regs, 1, idx, address, read_value);
}

static int vgic_mmio_write(struct vdev *vdev, gp_regs *regs, int idx,
		unsigned long address, unsigned long *write_value)
{
	return vgic_mmio_handler(vdev, regs, 0, idx, address, write_value);
}

static void vgic_gicd_init(struct vm *vm, struct vgic_gicd *gicd,
		unsigned long base, size_t size)
{
	uint32_t typer = 0;
	int nr_spi;

	/*
	 * when a vm is created need to create
	 * one vgic for each vm since gicr is percpu
	 * but gicd is shared so created it here
	 */
	memset(gicd, 0, sizeof(*gicd));

	gicd->base = base;
	gicd->end = base + size;

	spin_lock_init(&gicd->gicd_lock);

	gicd->gicd_ctlr = 0;

	/* GICV3 and provide vm->virq_nr interrupt */
	gicd->gicd_pidr2 = (0x3 << 4);

	typer |= vm->vcpu_nr << 5;
	typer |= 9 << 19;
	nr_spi = ((vm->vspi_nr + 32) >> 5) - 1;
	typer |= nr_spi;
	gicd->gicd_typer = typer;
}


static void vgic_gicr_init(struct vcpu *vcpu,
		struct vgic_gicr *gicr, unsigned long base)
{
	gicr->vcpu_id = get_vcpu_id(vcpu);

	/*
	 * now for gicv3 TBD, do not support vlpi.
	 */
	base = (128 * 1024) * vcpu->vcpu_id;
	gicr->rd_base = base;
	gicr->sgi_base = base + (64 * 1024);
	gicr->vlpi_base = 0;

	gicr->gicr_ctlr = 0;
	gicr->gicr_ispender = 0;
	spin_lock_init(&gicr->gicr_lock);

	/* TBD */
	gicr->gicr_pidr2 = 0x3 << 4;

	/*
	 * Linux will use the Last bit (bit 4) to detect whether
	 * this gicr is the last GICR.
	 */
	gicr->gicr_typer = 0 | ((unsigned long)vcpu->vcpu_id << 32);
	if (vcpu->vcpu_id == (vcpu->vm->vcpu_nr - 1))
		gicr->gicr_typer |= (1 << 4);
}

static void vm_release_gic(struct vgicv3_dev *gic)
{
	int i;
	struct vm *vm = gic->vdev.vm;
	struct vgic_gicr *gicr;

	if (!gic)
		return;

	vdev_release(&gic->vdev);

	for (i = 0; i < vm->vcpu_nr; i++) {
		gicr = gic->gicr[i];
		if (gicr)
			free(gicr);
	}

	free(gic);
}

static void vgic_deinit(struct vdev *vdev)
{
	struct vgicv3_dev *dev = vdev_to_vgic(vdev);

	return vm_release_gic(dev);
}

static void vgic_reset(struct vdev *vdev)
{
	pr_notice("vgic device reset\n");
}

static int64_t gicv3_read_lr(int lr)
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
}

static int gicv3_send_virq(struct vcpu *vcpu, struct virq_desc *virq)
{
	uint64_t value = 0;
	struct gic_lr *lr = (struct gic_lr *)&value;

	if (virq->id >= gicv3_nr_lr) {
		pr_err("invalid virq id %d\n", virq->id);
		return -EINVAL;
	}

	lr->v_intid = virq->vno;
	lr->p_intid = virq->hno;
	lr->priority = virq->pr;
	lr->group = 1;
	lr->hw = !!virq_is_hw(virq);
	lr->state = 1;

	gicv3_write_lr(virq->id, value);

	return 0;
}

static int gicv3_update_virq(struct vcpu *vcpu,
		struct virq_desc *desc, int action)
{
	if (!desc || desc->id >= gicv3_nr_lr)
		return -EINVAL;

	switch (action) {
		/*
		 * wether need to update the context value?
		 * TBD, since the context has not been saved
		 * so do not need to update it.
		 *
		 * 2: if the virq is attached to a physical irq
		 *    need to update the GICR register ?
		 */

	case VIRQ_ACTION_REMOVE:
		if (virq_is_hw(desc))
			irq_clear_pending(desc->hno);

	case VIRQ_ACTION_CLEAR:
		gicv3_write_lr(desc->id, 0);
		break;

	default:
		break;
	}

	return 0;
}

static int gicv3_get_virq_state(struct vcpu *vcpu, struct virq_desc *virq)
{
	uint64_t value;

	if (virq->id >= gicv3_nr_lr)
		return 0;

	value = gicv3_read_lr(virq->id);
	rmb();
	value = (value >> 62) & 0x03;

	return ((int)value);
}

static int gicv3_generate_virq(uint32_t *array, int virq)
{
	return vgic_generate_virq(array, virq);
}

static int vgicv3_vcpu_init(struct vcpu *vcpu, void *d, unsigned long flags)
{
	if (!(flags & VIRQCHIP_F_HW_VIRT))
		return 0;

	vcpu->virq_struct->nr_lrs = gicv3_nr_lr;

	return 0;
}

static void vgicv3_init_virqchip(struct virq_chip *vc,
		struct vgicv3_dev *dev, unsigned long flags)
{
	if (flags & VIRQCHIP_F_HW_VIRT) {
		vc->exit_from_guest = vgic_irq_exit_from_guest;
		vc->enter_to_guest = vgic_irq_enter_to_guest;
		vc->xlate = gic_xlate_irq;
		vc->generate_virq = gicv3_generate_virq;
		vc->send_virq = gicv3_send_virq;
		vc->update_virq = gicv3_update_virq;
		vc->get_virq_state = gicv3_get_virq_state;
		vc->vcpu_init = vgicv3_vcpu_init;
		vc->flags = flags;
	} else {
		pr_warn("***WARN***vgicv3 currently only" \
				"support hard virt mode\n");
	}

	vc->inc_pdata = dev;
}

static int get_vgicv3_info(struct device_node *node, struct vgicv3_info *vinfo)
{
	int ret;

	memset(vinfo, 0, sizeof(struct vgicv3_info));
	ret = translate_device_address_index(node, &vinfo->gicd_base,
			&vinfo->gicd_size, 0);
	if (ret) {
		pr_err("no gicv3 address info found\n");
		return -ENOENT;
	}

	ret = translate_device_address_index(node, &vinfo->gicr_base,
			&vinfo->gicr_size, 1);
	if (ret) {
		pr_err("no gicr address info found\n");
		return -ENOENT;
	}

	if (vinfo->gicd_base == 0 || vinfo->gicd_size == 0 ||
			vinfo->gicr_base == 0 || vinfo->gicr_size == 0) {
		pr_err("gicd or gicr address info not correct\n");
		return -EINVAL;
	}

	translate_device_address_index(node, &vinfo->gicc_base,
			&vinfo->gicc_size, 2);
	translate_device_address_index(node, &vinfo->gich_base,
			&vinfo->gich_size, 3);
	translate_device_address_index(node, &vinfo->gicv_base,
			&vinfo->gicv_size, 4);
	pr_notice("vgicv3: address 0x%x 0x%x 0x%x 0x%x\n",
			vinfo->gicd_base, vinfo->gicd_size,
			vinfo->gicr_base, vinfo->gicr_size);

	return 0;
}

struct virq_chip *vgicv3_virqchip_init(struct vm *vm,
		struct device_node *node)
{
	int i, ret = 0;
	struct vgic_gicr *gicr;
	struct vgicv3_dev *vgicv3_dev;
	struct vcpu *vcpu;
	struct vgicv3_info vinfo;
	unsigned long flags = 0;
	struct virq_chip *vc;
	struct arm_virt_data *arm_data = vm->arch_data;
	struct vdev *vdev;

	pr_notice("vgicv3: create vdev for vm-%d\n", vm->vmid);

	ret = get_vgicv3_info(node, &vinfo);
	if (ret)
		return NULL;

	vgicv3_dev = zalloc(sizeof(struct vgicv3_dev));
	if (!vgicv3_dev)
		return NULL;

	/*
	 * need create or init the vdev before add memory region
	 * to it.
	 */
	vdev = &vgicv3_dev->vdev;
	host_vdev_init(vm, vdev, "vgicv3");

	ret = vdev_add_iomem_range(vdev, vinfo.gicd_base, vinfo.gicd_size);
	ret += vdev_add_iomem_range(vdev, vinfo.gicr_base, vinfo.gicr_size);
	ret += vdev_add_iomem_range(vdev, vinfo.gicc_base, vinfo.gicc_size);
	ret += vdev_add_iomem_range(vdev, vinfo.gich_base, vinfo.gich_size);
	ret += vdev_add_iomem_range(vdev, vinfo.gicv_base, vinfo.gicv_size);
	if (ret) {
		pr_err("request vmm area for gicv3 failed\n");
		goto release_gic;
	}

	/*
	 * Init the vgic3_gicd and vgicv3_gicr.
	 */
	vgic_gicd_init(vm, &vgicv3_dev->gicd, vinfo.gicd_base, vinfo.gicd_size);
	for (i = 0; i < vm->vcpu_nr; i++) {
		vcpu = vm->vcpus[i];
		gicr = malloc(sizeof(struct vgic_gicr));
		if (!gicr)
			goto release_gic;

		vgic_gicr_init(vcpu, gicr, vinfo.gicr_base);
		vgicv3_dev->gicr[i] = gicr;
	}

	vdev->read = vgic_mmio_read;
	vdev->write = vgic_mmio_write;
	vdev->deinit = vgic_deinit;
	vdev->reset = vgic_reset;
	vdev_add(vdev);

	vc = alloc_virq_chip();
	if (!vc)
		goto release_gic;

	if (vgicv3_info.gicd_base != 0)
		flags |= VIRQCHIP_F_HW_VIRT;

	vgicv3_init_virqchip(vc, vgicv3_dev, flags);
	arm_data->sgi1r_el1_trap = vgicv3_send_sgi;

	return vc;

release_gic:
	vm_release_gic(vgicv3_dev);
	return NULL;
}
VIRQCHIP_DECLARE(vgicv3_chip, gicv3_match_table, vgicv3_virqchip_init);

static void gicv3_save_lrs(struct gicv3_context *c, uint32_t count)
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

static void gicv3_save_aprn(struct gicv3_context *c, uint32_t count)
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
	struct gicv3_context *c = (struct gicv3_context *)context;

	dsb();
	gicv3_save_lrs(c, gicv3_nr_lr);
	gicv3_save_aprn(c, gicv3_nr_pr);
	c->icc_sre_el1 = read_sysreg32(ICC_SRE_EL1);
	c->ich_vmcr_el2 = read_sysreg32(ICH_VMCR_EL2);
	c->ich_hcr_el2 = read_sysreg32(ICH_HCR_EL2);
}

static void gicv3_restore_aprn(struct gicv3_context *c, uint32_t count)
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

static void gicv3_restore_lrs(struct gicv3_context *c, uint32_t count)
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
	struct gicv3_context *c = (struct gicv3_context *)context;

	gicv3_restore_lrs(c, gicv3_nr_lr);
	gicv3_restore_aprn(c, gicv3_nr_pr);
	write_sysreg32(c->icc_sre_el1, ICC_SRE_EL1);
	write_sysreg32(c->ich_vmcr_el2, ICH_VMCR_EL2);
	write_sysreg32(c->ich_hcr_el2, ICH_HCR_EL2);
	dsb();
}

static void gicv3_state_init(struct vcpu *vcpu, void *context)
{
	struct gicv3_context *c = (struct gicv3_context *)context;

	memset(c, 0, sizeof(*c));
	c->icc_sre_el1 = 0x7;
	c->ich_vmcr_el2 = GICH_VMCR_VENG1 | (0xff << 24);
	c->ich_hcr_el2 = GICH_HCR_EN;
}

static void gicv3_state_resume(struct vcpu *vcpu, void *context)
{
	gicv3_state_init(vcpu, context);
}

static int gicv3_vmodule_init(struct vmodule *vmodule)
{
	vmodule->context_size = sizeof(struct gicv3_context);
	vmodule->state_init = gicv3_state_init;
	vmodule->state_save = gicv3_state_save;
	vmodule->state_restore = gicv3_state_restore;
	vmodule->state_resume = gicv3_state_resume;

	return 0;
}

int vgicv3_init(uint64_t *data, int len)
{
	int i;
	uint32_t val;
	unsigned long *value = (unsigned long *)&vgicv3_info;

	for (i = 0; i < len; i++)
		value[i] = (unsigned long)data[i];

	if (vgicv3_info.gicd_base == 0 || vgicv3_info.gicr_base == 0 ||
		vgicv3_info.gicd_size == 0 || vgicv3_info.gicr_size == 0)
		panic("invalid gicv3 register base from irqchip\n");

	val = read_sysreg32(ICH_VTR_EL2);
	gicv3_nr_lr = (val & 0x3f) + 1;
	gicv3_nr_pr = ((val >> 29) & 0x7) + 1;
	pr_notice("vgicv3: nr_lrs %d nr_prs %d\n", gicv3_nr_lr, gicv3_nr_pr);

	register_vcpu_vmodule("gicv3-vmodule", gicv3_vmodule_init);

	return 0;
}
