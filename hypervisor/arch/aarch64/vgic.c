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
#include <asm/gicv3.h>
#include <asm/io.h>
#include <minos/vmodule.h>
#include <minos/cpumask.h>
#include <minos/irq.h>
#include <asm/vgic.h>
#include <minos/sched.h>
#include <minos/virq.h>
#include <minos/vdev.h>
#include <asm/of.h>

#define vdev_to_vgic(vdev) \
	(struct vgic_dev *)container_of(vdev, struct vgic_dev, vdev)

void vgic_send_sgi(struct vcpu *vcpu, unsigned long sgi_value)
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
		send_virq_to_vcpu(target, sgi);
	}
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

static int vgic_mmio_handler(struct vdev *vdev, gp_regs *regs, int read,
		unsigned long address, unsigned long *value)
{
	int i;
	int type = GIC_TYPE_INVAILD;
	unsigned long offset;
	struct vgic_gicd *gicd = NULL;
	struct vgic_gicr *gicr = NULL;
	struct vcpu *vcpu = current_vcpu;
	struct vgic_dev *gic = vdev_to_vgic(vdev);

	gicr = gic->gicr[get_vcpu_id(vcpu)];
	gicd = &gic->gicd;

	if ((address >= gicd->base) && (address < gicd->end)) {
		type = GIC_TYPE_GICD;
		offset = address - gicd->base;
	} else {
		type = address_to_gicr(gicr, address, &offset);
		if (type != GIC_TYPE_INVAILD)
			goto out;

		/* master vcpu may access other vcpu's gicr */
		for (i = 0; i < vcpu->vm->vcpu_nr; i++) {
			gicr = gic->gicr[i];
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

static int vgic_mmio_read(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *read_value)
{
	return vgic_mmio_handler(vdev, regs, 1, address, read_value);
}

static int vgic_mmio_write(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *write_value)
{
	return vgic_mmio_handler(vdev, regs, 0, address, write_value);
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
	memset((char *)gicd, 0, sizeof(struct vgic_gicd));

	gicd->base = base;
	gicd->end = base + size;

	spin_lock_init(&gicd->gicd_lock);

	gicd->gicd_ctlr = 0;

	/* GICV3 and provide vm->virq_nr interrupt */
	gicd->gicd_pidr2 = (0x3 << 4);

	typer |= vm->vcpu_nr << 5;
	typer |= 9 << 19;
	nr_spi = (vm->vspi_nr >> 5) - 1;
	typer |= nr_spi;
	gicd->gicd_typer = typer;
}


static void vgic_gicr_init(struct vcpu *vcpu,
		struct vgic_gicr *gicr, unsigned long base)
{
	gicr->vcpu_id = get_vcpu_id(vcpu);
	/*
	 * now for gicv3 TBD
	 */
	base = base + (128 * 1024) * vcpu->vcpu_id;
	gicr->rd_base = base;
	gicr->sgi_base = base + (64 * 1024);
	gicr->vlpi_base = 0;

	gicr->gicr_ctlr = 0;
	gicr->gicr_ispender = 0;
	spin_lock_init(&gicr->gicr_lock);

	/* TBD */
	gicr->gicr_typer = 0 | ((unsigned long)vcpu->vcpu_id << 32);
	gicr->gicr_pidr2 = 0x3 << 4;
}

static void vm_release_gic(struct vgic_dev *gic)
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
	struct vgic_dev *dev = vdev_to_vgic(vdev);

	return vm_release_gic(dev);
}

static void vgic_reset(struct vdev *vdev)
{
	pr_info("vgic device reset\n");

	/* do nothing when reset the vm */
}

int vgic_create_vm(void *item, void *arg)
{
	int i, ret = 0;
	struct vgic_gicr *gicr;
	struct vm *vm = (struct vm *)item;
	struct vgic_dev *vgic_dev;
	struct vcpu *vcpu;
	uint64_t array[16];
	uint64_t size;

	vgic_dev = zalloc(sizeof(struct vgic_dev));
	if (!vgic_dev)
		return -ENOMEM;

	/*
	 * for host vm, get the gic's iomem space from
	 * the dtb but for gvm, emulator the system of
	 * arm FVP
	 */
	if (vm->vmid == 0) {
		i = 16;
		memset(array, 0, sizeof(array));
		ret = of_get_interrupt_regs(array, &i);
		if ((ret < 0) || (i < 4))
			return -ENOENT;

		/* count the iomem size of gic */
		size = array[2] + array[3] - array[0];
		host_vdev_init(vm, &vgic_dev->vdev, array[0], size);
	} else
		host_vdev_init(vm, &vgic_dev->vdev,
				GVM_VGIC_IOMEM_BASE,
				GVM_VGIC_IOMEM_SIZE);

	vdev_set_name(&vgic_dev->vdev, "vgic");

	if (vm->vmid == 0)
		vgic_gicd_init(vm, &vgic_dev->gicd, array[0], array[1]);
	else
		vgic_gicd_init(vm, &vgic_dev->gicd, GVM_VGICD_IOMEM_BASE,
				GVM_VGICD_IOMEM_SIZE);

	for (i = 0; i < vm->vcpu_nr; i++) {
		vcpu = vm->vcpus[i];
		gicr = malloc(sizeof(struct vgic_gicr));
		if (!gicr) {
			ret = -ENOMEM;
			goto release_gic;
		}

		if (vm->vmid == 0)
			vgic_gicr_init(vcpu, gicr, array[2]);
		else
			vgic_gicr_init(vcpu, gicr, GVM_VGICR_IOMEM_BASE);

		vgic_dev->gicr[i] = gicr;
	}

	vgic_dev->vdev.read = vgic_mmio_read;
	vgic_dev->vdev.write = vgic_mmio_write;
	vgic_dev->vdev.deinit = vgic_deinit;
	vgic_dev->vdev.reset = vgic_reset;

	/* here we put the vgic to header vdev_list */
	list_del(&vgic_dev->vdev.list);
	list_add(&vm->vdev_list, &vgic_dev->vdev.list);

	return 0;

release_gic:
	vm_release_gic(vgic_dev);
	return ret;
}

static int vgic_init(void)
{
	return register_hook(vgic_create_vm, MINOS_HOOK_TYPE_CREATE_VM_VDEV);
}

module_initcall(vgic_init);
