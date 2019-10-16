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
#include <asm/io.h>
#include <minos/vmodule.h>
#include <minos/cpumask.h>
#include <minos/irq.h>
#include <minos/sched.h>
#include <virt/virq.h>
#include <virt/vdev.h>
#include <minos/of.h>
#include <virt/virq_chip.h>
#include <asm/bcm_irq.h>

struct bcm2836_virq {
	struct vdev vdev;
	void *iomem;
	void *bcm2835_pending[NR_BANKS];
	void *bcm2835_enable[NR_BANKS];
	void *bcm2835_disable[NR_BANKS];
	int bcm2835_pendings;;
};

#define vdev_to_bcm_virq(vdev) \
	(struct bcm2836_virq *)container_of(vdev, struct bcm2836_virq, vdev)

extern int vgicv2_create_vm(void *item, void *arg);
static int bcm2836_clear_spi(struct vcpu *vcpu, uint32_t virq);
static int bcm2836_clear_ppi(struct vcpu *vcpu, uint32_t virq);

extern int bcm2836_xlate_irq(struct device_node *node,
		uint32_t *intspec, unsigned int intsize,
		uint32_t *hwirq, unsigned long *type);

static void bcm2836_virq_deinit(struct vdev *vdev)
{
	struct bcm2836_virq *bcm2836 = vdev_to_bcm_virq(vdev);

	free(bcm2836->iomem);
	vdev_release(vdev);
	free(bcm2836);
}

static void bcm2836_virq_reset(struct vdev *vdev)
{
	struct bcm2836_virq *bcm2836 = vdev_to_bcm_virq(vdev);

	memset(bcm2836->iomem, 0, PAGE_SIZE);
}

static int bcm2836_virq_read(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *read_value)
{
	/* guest can directly read the memory space */

	return 0;
}

static void inline bcm2835_virq_enable(struct vcpu *vcpu,
		int base, unsigned long *value)
{
	uint32_t bit;

	for_each_set_bit(bit, value, 32)
		virq_enable(vcpu, base + bit);
}

static void inline bcm2835_virq_disable(struct vcpu *vcpu,
		int base, unsigned long *value)
{
	uint32_t bit;

	for_each_set_bit(bit, value, 32)
		virq_disable(vcpu, base + bit);
}

static int bcm2835_virq_write(struct vdev *vdev, gp_regs *reg,
		unsigned long offset, unsigned long *value)
{
	uint32_t irq;
	struct vcpu *vcpu = get_current_vcpu();

	switch (offset) {
	case BCM2835_IRQ_ACK:
		irq = *value + 32;
		bcm2836_clear_spi(vcpu, irq);
		clear_pending_virq(vcpu, irq);
		break;
	case BCM2835_IRQ_ENABLE1:
		bcm2835_virq_enable(vcpu, 64, value);
		break;
	case BCM2835_IRQ_ENABLE2:
		bcm2835_virq_enable(vcpu, 96, value);
		break;
	case BCM2835_IRQ_BASIC_ENABLE:
		bcm2835_virq_enable(vcpu, 32, value);
		break;
	case BCM2835_IRQ_DISABLE1:
		bcm2835_virq_disable(vcpu, 64, value);
		break;
	case BCM2835_IRQ_DISABLE2:
		bcm2835_virq_disable(vcpu, 96, value);
		break;
	case BCM2835_IRQ_DISABLE_BASIC:
		bcm2835_virq_disable(vcpu, 32, value);
		break;
	default:
		pr_warn("unsupport bcm2835 action now 0x%x\n", offset);
		break;
	}

	return 0;
}

static int inline bcm2836_send_vsgi(struct vcpu *vcpu, struct vdev *vdev,
		unsigned long offset, unsigned long *value)
{
	int sgi;
	struct vcpu *target;
	struct vm *vm = vcpu->vm;
	int cpu = (offset - LOCAL_MAILBOX0_SET0) / 16;

	if (*value == 0)
		return -EINVAL;

	sgi = __ffs((uint32_t)*value);
	pr_debug("send vsgi %d %d\n", cpu, sgi);

	if ((cpu > vm->vcpu_nr) || (sgi >= 16))
		return -EINVAL;

	target = get_vcpu_in_vm(vm, cpu);
	if (!target)
		return -EINVAL;

	return send_virq_to_vcpu(target, sgi);
}

static int inline bcm2836_clear_vsgi(struct vcpu *vcpu, struct vdev *vdev,
		unsigned long offset, unsigned long *value)
{
	uint32_t v;
	int sgi;
	int cpu = (offset - LOCAL_MAILBOX0_CLR0) / 16;
	void *base;
	struct vcpu *target;
	struct vm *vm = vcpu->vm;
	struct bcm2836_virq *dev = vdev_to_bcm_virq(vdev);

	if (*value == 0)
		return -EINVAL;

	sgi = __ffs((uint32_t)*value);
	pr_debug("clear vsgi %d %d\n", cpu, sgi);

	if ((cpu > vm->vcpu_nr) || (sgi >= 16))
		return -EINVAL;

	target = get_vcpu_in_vm(vm, cpu);
	if (!target)
		return -EINVAL;

	v = readl_relaxed(dev->iomem + offset);
	v &= ~((uint32_t)*value);
	writel_relaxed(v, dev->iomem + offset);

	/* clear the LOCAL_MAILBOX0 pending bit */
	if (v == 0) {
		base = dev->iomem + LOCAL_IRQ_PENDING0 + 4 * cpu;
		v = readl_relaxed(base) & ~BIT(LOCAL_IRQ_MAILBOX0);
		writel_relaxed(v, base);
	}

	clear_pending_virq(vcpu, sgi);
	return 0;
}

static int bcm2836_timer_int_action(struct vcpu *vcpu, struct vdev *vdev,
		unsigned long offset, unsigned long *value)
{
	int i;
	uint32_t v = (uint32_t)*value;
	struct vcpu *target;
	int cpu = (offset - LOCAL_TIMER_INT_CONTROL0) / 4;
	struct bcm2836_virq *dev = vdev_to_bcm_virq(vdev);

	target = get_vcpu_in_vm(vcpu->vm, cpu);
	if (!target)
		return -EINVAL;

	/* update the value for vm read */
	writel_relaxed(v, dev->iomem + offset);

	/* timer treated as ppi in the system */
	for (i = 0; i < 4; i++) {
		if (v & (1 << i))
			virq_enable(vcpu, i + 16);
		else
			virq_disable(vcpu, i + 16);
	}

	return 0;
}

static int bcm2836_virq_write(struct vdev *vdev, gp_regs *reg,
		unsigned long address, unsigned long *write_value)
{
	unsigned long offset = address - BCM2836_INC_BASE;
	struct vcpu *vcpu = get_current_vcpu();
	int cpu, irq;

	if (offset >= BCM2835_INC_OFFSET)
		return bcm2835_virq_write(vdev, reg, offset, write_value);

	switch (offset) {
	case LOCAL_CONTROL:
	case LOCAL_PRESCALER:
		break;
	case BCM2836_IRQ_ACK:
		irq = *write_value;
		bcm2836_clear_ppi(vcpu, irq);
		clear_pending_virq(vcpu, irq + 16);
		break;
	case LOCAL_MAILBOX_INT_CONTROL0...LOCAL_MAILBOX_INT_CONTROL3:
		/* mailbox interrupt aloways enabled */
		break;

	case LOCAL_TIMER_INT_CONTROL0...LOCAL_TIMER_INT_CONTROL3:
		bcm2836_timer_int_action(vcpu, vdev, offset, write_value);
		break;

	case LOCAL_MAILBOX_SET_START...LOCAL_MAILBOX_SET_END:
		/* send the ipi now only using mailbox0 */
		bcm2836_send_vsgi(vcpu, vdev, offset, write_value);
		break;

	case LOCAL_MAILBOX_CLR_START...LOCAL_MAILBOX_CLR_END:
		bcm2836_clear_vsgi(vcpu, vdev, offset, write_value);
		break;

	/* used to spin table to boot up vm0's vcpu */
	case BCM2836_RELEASE_OFFSET...BCM2836_RELEASE_OFFSET_END:
		cpu = (offset - BCM2836_RELEASE_OFFSET) / sizeof(uint64_t);
		if (cpu == 0) {
			pr_err("vcpu0 has alreadly power on\n");
			break;
		}
		vcpu_power_on(vcpu, cpuid_to_affinity(cpu), *write_value, 0);
		break;
	}

	return 0;
}

static int bcm2836_inject_sgi(struct vcpu *vcpu, uint32_t virq)
{
	void *base;
	uint32_t v;
	struct vm *vm = vcpu->vm;
	struct bcm2836_virq *dev;

	dev = (struct bcm2836_virq *)vm->virq_chip->inc_pdata;
	base = dev->iomem + (LOCAL_MAILBOX0_CLR0 + vcpu->vcpu_id * 16);

	/* set the read and clear register */
	v = readl_relaxed(base);
	writel_relaxed(v | (1 << virq), base);

	/* set the LOCAL_IRQ_MAILBOX0 bit of LOCAL_IRQ_PENDING0 */
	base = dev->iomem + LOCAL_IRQ_PENDING0 + 4 * vcpu->vcpu_id;
	v = readl_relaxed(base) | BIT(LOCAL_IRQ_MAILBOX0);
	writel_relaxed(v, base);

	return 0;
}

static int bcm2836_inject_ppi(struct vcpu *vcpu, uint32_t virq)
{
	void *base;
	uint32_t v;
	struct vm *vm = vcpu->vm;
	struct bcm2836_virq *dev;

	virq = virq - 16;
	if (virq == LOCAL_IRQ_GPU_FAST) {
		pr_warn("%d irq is for bcm2835 inc\n");
		return -EINVAL;
	}

	dev = (struct bcm2836_virq *)vm->virq_chip->inc_pdata;
	base = dev->iomem + LOCAL_IRQ_PENDING0 + 4 * vcpu->vcpu_id;
	v = readl_relaxed(base) | BIT(virq);
	writel_relaxed(v, base);

	return 0;
}

static int bcm2836_get_spi_bank(struct bcm2836_virq*dev,
		uint32_t virq, void **iomem, int *pos, int *__bank)
{
	int bank, bit;
	void *base;

	*__bank = bank = HWIRQ_BANK(virq);
	virq = virq % 32;
	if (bank >= NR_BANKS)
		return -EINVAL;

	if (bank == 0) {
		if (virq >= 10 && virq <= 20) {
			pr_warn("10 - 20 virq in bank0 is for other\n");
			return -EINVAL;
		}

		base = dev->bcm2835_pending[0];
		bit = virq;
	} else if (bank == 1) {
		switch (virq) {
		case 19:
			base = dev->bcm2835_pending[0];
			bit = 14;
			break;
		case 18:
			base = dev->bcm2835_pending[0];
			bit = 13;
			break;
		case 10:
			base = dev->bcm2835_pending[0];
			bit = 12;
			break;
		case 9:
			base = dev->bcm2835_pending[0];
			bit = 11;
			break;
		case 7:
			base = dev->bcm2835_pending[0];
			bit = 10;
			break;
		default:
			base = dev->bcm2835_pending[1];
			bit = virq;
			break;
		}
	} else {
		switch (virq) {
		case 30:
			base = dev->bcm2835_pending[0];
			bit = 20;
			break;
		case 25:
			base = dev->bcm2835_pending[0];
			bit = 19;
			break;
		case 24:
			base = dev->bcm2835_pending[0];
			bit = 18;
			break;
		case 23:
			base = dev->bcm2835_pending[0];
			bit = 17;
			break;
		case 22:
			base = dev->bcm2835_pending[0];
			bit = 16;
			break;
		case 21:
			base = dev->bcm2835_pending[0];
			bit = 15;
			break;
		default:
			base = dev->bcm2835_pending[2];
			bit = virq;
			break;
		}
	}

	*iomem = base;
	*pos = bit;

	return 0;
}

static int bcm2836_clear_spi(struct vcpu *vcpu, uint32_t virq)
{
	int bit, bank;
	uint32_t v, p0, p1, p2;
	void *base;
	struct vm *vm = vcpu->vm;
	struct bcm2836_virq *dev;

	dev = (struct bcm2836_virq *)vm->virq_chip->inc_pdata;
	if (bcm2836_get_spi_bank(dev, virq, &base, &bit, &bank)) {
		pr_err("get irq bank failed %d\n", virq);
		return -EINVAL;
	}

	v = readl_relaxed(base) & ~BIT(bit);
	writel_relaxed(v, base);

	p0 = readl_relaxed(dev->bcm2835_pending[0]);
	p1 = readl_relaxed(dev->bcm2835_pending[1]);
	p2 = readl_relaxed(dev->bcm2835_pending[2]);

	if (p1 == 0)
		p0 &= ~BIT(8);
	if (p2 == 0)
		p0 &= ~BIT(9);

	writel_relaxed(p0, dev->bcm2835_pending[0]);
	return 0;
}

static int bcm2836_clear_ppi(struct vcpu *vcpu, uint32_t virq)
{
	uint32_t v;
	void *base;
	struct vm *vm = vcpu->vm;
	struct bcm2836_virq *dev;

	if (virq > LOCAL_IRQ_PMU_FAST)
		return -EINVAL;

	dev = (struct bcm2836_virq *)vm->virq_chip->inc_pdata;
	base = dev->iomem + LOCAL_IRQ_PENDING0 + (vcpu->vcpu_id * 4);
	v = readl_relaxed(base) & ~BIT(virq);
	writel_relaxed(v, base);

	return 0;
}

static int bcm2836_inject_spi(struct vcpu *vcpu, uint32_t virq)
{
	int bit, bank;
	uint32_t v;
	void *base;
	struct vm *vm = vcpu->vm;
	struct bcm2836_virq *dev;

	dev = (struct bcm2836_virq *)vm->virq_chip->inc_pdata;
	if (bcm2836_get_spi_bank(dev, virq, &base, &bit, &bank)) {
		pr_info("get virq bank failed %d\n", virq);
		return -EINVAL;
	}

	if (bank > 0) {
		/* update the bank register bit */
		v = readl_relaxed(dev->iomem + BCM2835_IRQ_BASIC_PENDING);
		v |= BIT(bank + 7);
		writel_relaxed(v, dev->iomem + BCM2835_IRQ_BASIC_PENDING);
	}

	v = readl_relaxed(base) | BIT(bit);
	writel_relaxed(v, base);

	/* set the bcm2836 pending register BIT8 */
	base = dev->iomem + LOCAL_IRQ_PENDING0;
	v = readl_relaxed(base) | BIT(8);
	writel_relaxed(v, base);

	return 0;
}

int bcm2836_send_virq(struct vcpu *vcpu, uint32_t virq)
{
	/* convert the hypervisor virq to bcm irq number */
	switch (virq) {
	case 0 ... 15:
		bcm2836_inject_sgi(vcpu, virq);
		break;
	case 16 ... 31:
		bcm2836_inject_ppi(vcpu, virq);
		break;
	case 32 ... 127:
		bcm2836_inject_spi(vcpu, virq);
		break;
	default:
		pr_err("unsupport bcm2836 virq number\n");
		break;
	}

	return 0;
}

static int bcm2836_vm0_virq_data(uint32_t *array, int vspi_nr, int type)
{
	int i, size = 0;

	if (type & VM_FLAGS_SETUP_OF) {
		for (i = 0; i < vspi_nr; i++) {
			*array++ = cpu_to_of32(i / 32);
			*array++ = cpu_to_of32(i % 32);
			size += (2 * 4);
		}
	}

	return size;
}

static int bcm2836_update_virq(struct vcpu *vcpu,
		struct virq_desc *desc, int action)
{
	switch (action) {
	case VIRQ_ACTION_CLEAR:
		/* enable the hardware irq when disable in hyp */
		if ((desc->vno >= 32) && (virq_is_hw(desc)))
			irq_unmask(desc->hno);
		break;
	}
	return 0;
}

static int bcm2836_enter_to_guest(struct vcpu *vcpu, void *data)
{
	struct virq_desc *virq, *n;
	struct virq_struct *virq_struct = vcpu->virq_struct;

	/*
	 * just inject one virq the time since when the
	 * virq is handled, the vm will trap to hypervisor
	 * again, actually here can inject all virq to the
	 * guest, but it is hard to judge whether all virq
	 * has been handled by guest vm TBD
	 */
	list_for_each_entry_safe(virq, n, &virq_struct->pending_list, list) {
		if (!virq_is_pending(virq)) {
			pr_err("virq is not request %d\n", virq->vno);
			list_del(&virq->list);
			virq->list.next = NULL;
			continue;
		}

#if 0
		/*
		 * virq is not enabled this time, need to
		 * send it later, but this will infence the idle
		 * condition jugement TBD
		 */
		if (!virq_is_enabled(virq))
			continue;
#endif

		/*
		 * update the virq interrupt status and
		 * delete the virq from the virq list then
		 * add it to active list
		 */
		bcm2836_send_virq(vcpu, virq->vno);
		virq_clear_pending(virq);
		virq->state = VIRQ_STATE_ACTIVE;
		list_del(&virq->list);
		list_add_tail(&virq_struct->active_list, &virq->list);
	}

	return 0;
}

static struct virq_chip *bcm2836_virqchip_init(struct vm *vm,
		struct device_node *node)
{
	struct bcm2836_virq *bcm2836;
	struct vdev *vdev;
	void *base;
	struct virq_chip *vc;

	bcm2836 = zalloc(sizeof(struct bcm2836_virq));
	if (!bcm2836)
		return NULL;

	bcm2836->iomem = get_io_page();
	if (!bcm2836->iomem) {
		free(bcm2836);
		return NULL;
	}

	base = bcm2836->iomem + 0x200;
	bcm2836->bcm2835_pending[0] = base + 0x0;
	bcm2836->bcm2835_pending[1] = base + 0x04;
	bcm2836->bcm2835_pending[2] = base + 0x08;
	bcm2836->bcm2835_enable[0] = base + 0x18;
	bcm2836->bcm2835_enable[1] = base + 0x10;
	bcm2836->bcm2835_enable[2] = base + 0x14;
	bcm2836->bcm2835_disable[0] = base + 0x24;
	bcm2836->bcm2835_disable[1] = base + 0x1c;
	bcm2836->bcm2835_disable[2] = base + 0x20;

	memset(bcm2836->iomem, 0, PAGE_SIZE);
	vdev = &bcm2836->vdev;
	host_vdev_init(vm, vdev, BCM2836_INC_BASE, PAGE_SIZE);
	vdev_set_name(vdev, "bcm2836-irq");
	vdev->read = bcm2836_virq_read;
	vdev->write = bcm2836_virq_write;
	vdev->reset = bcm2836_virq_reset;
	vdev->deinit = bcm2836_virq_deinit;

	/*
	 * map the io space to guest as read only Notice :
	 * bcm2836 base address is 7e00b200 which is not
	 * PAGE ALIG
	 *
	 * here map the bcm2835 and bcm2836 interrupt controller
	 * space all to 0x40000000
	 * 0x40000000 - 0x40000100 : bcm2836 local interrupt
	 * 0x40000200 - 0x40000300 : bcm2835 inc controller
	 *
	 */
	create_guest_mapping(&vm->mm, BCM2836_INC_BASE, (unsigned long)bcm2836->iomem,
			PAGE_SIZE, VM_IO | VM_RO);

	vc = alloc_virq_chip();
	if (!vc)
		return NULL;

	vc->xlate = bcm2836_xlate_irq;
	vc->exit_from_guest = NULL;
	vc->enter_to_guest = bcm2836_enter_to_guest;
	vc->vm0_virq_data = bcm2836_vm0_virq_data;
	vc->update_virq = bcm2836_update_virq;
	vc->inc_pdata = (void *)bcm2836;

	return vc;
}
VIRQCHIP_DECLARE(bcm2836_virqchip, bcmirq_match_table,
		bcm2836_virqchip_init);
