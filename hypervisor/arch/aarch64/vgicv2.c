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
#include <minos/virq.h>
#include <minos/vdev.h>
#include <asm/of.h>

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

static struct vgicv2_info vgicv2_info;

#define vdev_to_vgicv2(vdev) \
	(struct vgicv2_dev *)container_of(vdev, struct vgicv2_dev, vdev)

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
		*value = 0;
		break;
	case GICD_ICENABLER...GICD_ICENABLERN:
		*value = 0;
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
		*value = 0;
		break;
	case GICD_ITARGETSR...GICD_ITARGETSR7:
		tmp = 1 << get_vcpu_id(vcpu);
		*value = tmp;
		*value |= tmp << 8;
		*value |= tmp << 16;
		*value |= tmp << 24;
		break;
	case GICD_ITARGETSR8...GICD_ITARGETSRN:
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

	cpumask_clear(&cpumask);
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
	struct vcpu *vcpu = current_vcpu;
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
	return vgicv2_mmio_handler(vdev, regs, 1, address, write_value);
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

int vgicv2_create_vm(void *item, void *arg)
{
	struct vm *vm = (struct vm *)item;
	struct vgicv2_dev *dev;
	unsigned long base, size;

	dev = zalloc(sizeof(struct vgicv2_dev));
	if (!dev)
		return -ENOMEM;

	if (vm_is_native(vm)) {
		base = vgicv2_info.gicd_base;
		size = vgicv2_info.gicd_size;
	} else {
		base = 0x2f000000;
		size = 0x10000;
	}

	dev->gicd_base = base;
	host_vdev_init(vm, &dev->vdev, base, size);
	vdev_set_name(&dev->vdev, "vgicv2");

	dev->gicd_typer = vm->vcpu_nr << 5;
	dev->gicd_typer |= (vm->vspi_nr >> 5) - 1;

	dev->gicd_iidr = 0x0;

	dev->vdev.read = vgicv2_mmio_read;
	dev->vdev.write = vgicv2_mmio_write;
	dev->vdev.deinit = vgicv2_deinit;
	dev->vdev.reset = vgicv2_reset;

	/* map the gicc memory for guest */
	if (vm_is_native(vm)) {
		base = vgicv2_info.gicc_base;
		size = vgicv2_info.gicc_size;
	} else {
		base = 0x2c000000;
		size = 0x2000;
	}

	create_guest_mapping(vm, base, vgicv2_info.gicc_base,
			size, VM_IO);

	return 0;
}

int vigcv2_init(uint64_t *data, int len)
{
	int i;
	unsigned long *value = (unsigned long *)&vgicv2_info;

	for (i = 0; i < len; i++)
		value[i] = (unsigned long)data[i];

	for (i = 0; i < 4; i++) {
		if (value[i] == 0)
			panic("invalid address of gicv2\n");
	}

	return register_hook(vgicv2_create_vm,
			MINOS_HOOK_TYPE_CREATE_VM_VDEV);
}
