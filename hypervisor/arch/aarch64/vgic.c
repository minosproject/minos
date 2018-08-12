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
#include <minos/mmio.h>
#include <minos/irq.h>
#include <asm/gicv3.h>
#include <minos/io.h>
#include <minos/vmodule.h>
#include <minos/cpumask.h>
#include <minos/irq.h>
#include <asm/vgic.h>
#include <minos/sched.h>
#include <minos/virq.h>

static DEFINE_SPIN_LOCK(gicd_lock);

static struct list_head gicd_list;
static int vgic_vmodule_id = INVAILD_MODULE_ID;

static struct vgic_gicd *attach_vgicd(uint32_t vmid)
{
	struct vgic_gicd *gicd;

	list_for_each_entry(gicd, &gicd_list, list) {
		if (gicd->vmid == vmid)
			return gicd;
	}

	return NULL;
}

void vgic_send_sgi(struct vcpu *vcpu, unsigned long sgi_value)
{
	sgi_mode_t mode;
	uint32_t sgi;
	cpumask_t cpumask;
	unsigned long tmp, aff3, aff2, aff1;
	int bit, logic_cpu;
	struct vm *vm = vcpu->vm;
	struct vcpu *target;
	struct vgic_gicr *gicr;

	sgi = (sgi_value & (0xf << 24)) >> 24;
	if (sgi >= 16) {
		pr_error("vgic : sgi number is incorrect %d\n", sgi);
		return;
	}

	mode = sgi_value & (1UL << 40) ? SGI_TO_OTHERS : SGI_TO_LIST;
	cpumask_clear(&cpumask);

	if (mode == SGI_TO_LIST) {
		tmp = sgi_value & 0xffff;
		aff3 = (sgi_value & (0xffUL << 48)) >> 48;
		aff2 = (sgi_value & (0xffUL << 32)) >> 32;
		aff1 = (sgi_value & (0xffUL << 16)) >> 16;
		for_each_set_bit(bit, &tmp, 16) {
			logic_cpu = affinity_to_logic_cpu(aff3, aff2, aff1, bit);
			cpumask_set_cpu(logic_cpu, &cpumask);
		}
	} else {
		for (bit = 0; bit < vm->vcpu_nr; bit++) {
			if (bit == vcpu->vcpu_id)
				continue;
			cpumask_set_cpu(bit, &cpumask);
		}
	}

	/*
	 * here we update the gicr releated register
	 * for some other purpose use TBD
	 */

	for_each_cpu(bit, &cpumask) {
		target = get_vcpu_in_vm(vm, bit);
		gicr = (struct vgic_gicr *)
			get_vmodule_data_by_id(target, vgic_vmodule_id);

		/*
		 * for the os which using sgi to wake
		 * up other core
		 */
		spin_lock(&gicr->gicr_lock);
		gicr->gicr_ispender |= (1 << sgi);
		spin_unlock(&gicr->gicr_lock);
		send_virq_to_vcpu(target, sgi);
	}
}

static void vgic_state_init(struct vcpu *vcpu, void *context)
{
	struct vgic_gicr *gicr = (struct vgic_gicr *)context;
	unsigned long base;

	gicr->gicd = attach_vgicd(get_vmid(vcpu));
	if (gicr->gicd == NULL) {
		pr_error("can not find gicd for this vcpu\n");
		return;
	}

	gicr->vcpu_id = get_vcpu_id(vcpu);

	/*
	 * now for gicv3 TBD
	 */
	base = 0x2f100000 + (128 * 1024) * vcpu->vcpu_id;
	gicr->rd_base = base;
	gicr->sgi_base = base + (64 * 1024);
	gicr->vlpi_base = 0;

	spin_lock_init(&gicr->gicr_lock);
	init_list(&gicr->list);
	list_add_tail(&gicr->gicd->gicr_list, &gicr->list);

	/*
	 * int the gicr
	 */
	gicr->gicr_ctlr = 0;
	gicr->gicr_ispender = 0;
	gicr->gicr_typer = ioread64((void *)gicr->rd_base + GICR_TYPER);
	gicr->gicr_pidr2 = ioread32((void *)gicr->rd_base + GICR_PIDR2);
}

static void vgic_state_save(struct vcpu *vcpu, void *context)
{

}

static void vgic_state_restore(struct vcpu *vcpu, void *context)
{

}

static void vgic_vm_deinit(struct vm *vm)
{
	struct vgic_gicd *gicd;

	gicd = attach_vgicd(vm->vmid);
	if (!gicd)
		pr_error("no gicd found for vm-%d\n", vm->vmid);

	spin_lock(&gicd_lock);
	list_del(&gicd->list);
	spin_unlock(&gicd_lock);

	free(gicd);
}

static void vgic_vm_init(struct vm *vm)
{
	struct vgic_gicd *gicd;

	/*
	 * when a vm is created need to create
	 * one vgic for each vm since gicr is percpu
	 * but gicd is shared so created it here
	 */
	gicd = (struct vgic_gicd *)malloc(sizeof(struct vgic_gicd));
	if (!gicd)
		panic("No more memory for gicd\n");

	memset((char *)gicd, 0, sizeof(struct vgic_gicd));

	gicd->vmid = vm->vmid;
	gicd->base = 0x2f000000;
	gicd->end = 0x2f010000;

	init_list(&gicd->list);
	init_list(&gicd->gicr_list);
	spin_lock_init(&gicd->gicd_lock);

	spin_lock(&gicd_lock);
	list_add_tail(&gicd_list, &gicd->list);
	spin_unlock(&gicd_lock);

	/*
	 * init gicd TBD
	 */
	gicd->gicd_ctlr = 0;
	gicd->gicd_pidr2 = ioread32((void *)gicd->base + GICD_PIDR2);
	gicd->gicd_typer = ioread32((void *)gicd->base + GICD_TYPER);
}

static int address_to_gicr(struct vgic_gicr *gicr,
		unsigned long address, unsigned long *offset)
{
	if ((address >= gicr->rd_base) &&
		(address < (gicr->rd_base + SIZE_64K))) {

		*offset = address - gicr->rd_base;
		return GIC_TYPE_GICR_RD;
	}

	if ((address >= gicr->sgi_base) &&
		(address < (gicr->sgi_base + SIZE_64K))) {

		*offset = address - gicr->sgi_base;
		return GIC_TYPE_GICR_SGI;
	}

	if ((address >= gicr->vlpi_base) &&
		(address < (gicr->vlpi_base + SIZE_64K))) {

		*offset = address - gicr->vlpi_base;
		return GIC_TYPE_GICR_VLPI;
	}

	return GIC_TYPE_INVAILD;
}

static int vgic_gicd_mmio_read(struct vcpu *vcpu,
			struct vgic_gicd *gicd,
			unsigned long offset,
			unsigned long *value)
{
	spin_lock(&gicd->gicd_lock);

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
		default:
			*value = 0;
			break;
	}

	spin_unlock(&gicd->gicd_lock);
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
			*value = 0x3 << 4;	// gicv3
			break;
		case GICR_TYPER:
			*value = gicr->gicr_typer;
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
				return 1;
			default:
				return 0;
			}
		} else if (type == GIC_TYPE_GICR_SGI) {
			return 0;
		} else if (type == GIC_TYPE_GICR_VLPI) {
			return 0;
		} else {
			return 0;
		}
	}

	return 1;
}

static int vgic_mmio_handler(gp_regs *regs, int read,
		unsigned long address, unsigned long *value)
{
	int type = GIC_TYPE_INVAILD;
	unsigned long offset;
	struct vgic_gicd *gicd = NULL;
	struct vgic_gicr *gicr = NULL;
	struct vcpu *vcpu = current_vcpu;

	gicr = (struct vgic_gicr *)
		get_vmodule_data_by_id(vcpu, vgic_vmodule_id);
	gicd = gicr->gicd;

	if ((address >= gicd->base) && (address < gicd->end)) {
		type = GIC_TYPE_GICD;
		offset = address - gicd->base;
	} else {
		type = address_to_gicr(gicr, address, &offset);
		if (type != GIC_TYPE_INVAILD)
			goto out;

		/*
		 * may access other vcpu's gicr register
		 */
		list_for_each_entry(gicr, &gicd->gicr_list, list) {
			type = address_to_gicr(gicr, address, &offset);
			if (type != GIC_TYPE_INVAILD)
				goto out;
		}
	}

out:
	if (type == GIC_TYPE_INVAILD) {
		pr_error("invaild gicr type and address\n");
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
		pr_error("unsupport gic type %d\n", type);
		return -EINVAL;
	}

	return 0;
}

static int vgic_mmio_read(gp_regs *regs,
		unsigned long address, unsigned long *read_value)
{
	return vgic_mmio_handler(regs, 1, address, read_value);
}

static int vgic_mmio_write(gp_regs *regs,
		unsigned long address, unsigned long *write_value)
{
	return vgic_mmio_handler(regs, 0, address, write_value);
}

static int vgic_mmio_check(gp_regs *regs, unsigned long address)
{
	if ((address >= 0x2f000000) && (address < 0x2f200000))
		return 1;

	return 0;
}

static struct mmio_ops vgic_mmio_ops = {
	.read = vgic_mmio_read,
	.write = vgic_mmio_write,
	.check = vgic_mmio_check,
};

static int vgic_vmodule_init(struct vmodule *vmodule)
{
	init_list(&gicd_list);

	vmodule->context_size = sizeof(struct vgic_gicr);
	vmodule->pdata = NULL;
	vmodule->state_init = vgic_state_init;
	vmodule->state_save = vgic_state_save;
	vmodule->state_restore = vgic_state_restore;
	vmodule->vm_init = vgic_vm_init;
	vmodule->vm_deinit = vgic_vm_deinit;
	vgic_vmodule_id = vmodule->id;

	register_mmio_emulation_handler("vgic", &vgic_mmio_ops);

	return 0;
}

MINOS_MODULE_DECLARE(vgic, "vgic", (void *)vgic_vmodule_init);
