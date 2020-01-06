/*
 * Copyright (C) 2019 Min Le (lemin9538@gmail.com)
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
 *
 * virtual interrupt driver for AIC (Apple Iterrupt Controller)76
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
#include <virt/resource.h>
#include <virt/virq_chip.h>
#include <minos/of.h>

#define AIC_REV				0x0000
#define AIC_CAP0			0x0004
#define AIC_CAP1			0x0008
#define AIC_RST				0x000c
#define AIC_CFG				0x0010
#define AIC_MAIN_TIME_LO		0x0020
#define AIC_MAIN_TIME_HI		0X0028
#define AIC_IPI_NORMAL_DBG		0x0030
#define AIC_IPI_SELF_DBG		0x0034

#define AIC_WHO_AM_I			0x2000
#define AIC_ACK				0x2004
#define AIC_IPI_SET			0x2008
#define AIC_IPI_CLR			0x200c
#define AIC_TMR_CFG			0x2010
#define AIC_TMR_CNT			0x2014
#define AIC_TMR_INT_STAT		0x2018
#define AIC_TMR_INT_STAT_SET		0x201c
#define AIC_TMR_INT_STAT_CLR		0x2020
#define AIC_BANKED_CORE_REGS		0x2000

#define AIC_CAP0_INT(n)			((n) & 0x3ff)
#define AIC_CAP0_PROC(n)		((((n) >> 16) & 0x1f) + 1)
#define AIC_ACK_VEC_TYPE(n)		(((n) >> 16) & 7)

#define AIC_ACK_VEC_TYPE_SPURIOUS	(0)
#define AIC_ACK_VEC_TYPE_EXT		(1)
#define AIC_ACK_VEC_TYPE_IPI		(4)
#define AIC_ACK_VEC_TYPE_TIMER		(7)
#define AIC_ACK_VEC_EXT_INT(n)		((n) & 0x3ff)
#define AIC_ACK_VEC_IPI_TYPE(n)		((n) & 0x003)
#define AIC_ACK_VEC_IPI_TYPE_NORMAL	(1)
#define AIC_ACK_VEC_IPI_TYPE_SELF	(2)

#define AIC_IPI_CLR_SELF		(0x80000000)

#define AIC_TMR_CFG_EN			1
#define AIC_TMR_CFG_FSL_PTI		(0 << 4)
#define AIC_TMR_CFG_FSL_SGTI		(1 << 4)
#define AIC_TMR_CFG_FSL_ETI		(2 << 4)
#define AIC_TMR_INT_STAT_PCT		(1)
#define AIC_BANED_CORE_TMR_CNT		0x14
#define AIC_BANKED_CORE_TMR_INT_STAT	0x18

#define AIC_TGT_DST(n)			(0x3000 + (n) * 4)
#define AIC_SWGEN_SET(n)		(0x4000 + (n) * 4)
#define AIC_SWGEN_CLR(n)		(0x4080 + (n) * 4)
#define AIC_INT_MASK_SET(n)		(0x4100 + (n) * 4)
#define AIC_INTMASK_CLR(n)		(0x4180 + (n) * 4)
#define AIC_HW_INT_MON(n)		(0x4200 + (n) * 4)

#define AIC_ALIAS_WHO_AM_I(n)		(0x5000 + (n) * 0x80 + 0x00)
#define AIC_ALIAS_INT_ACK(n)		(0x5000 + (n) * 0x80 + 0x04)
#define AIC_ALIAS_IPI_SET(n)		(0x5000 + (n) * 0x80 + 0x08)
#define AIC_ALIAS_IPI_CLR(n)		(0x5000 + (n) * 0x80 + 0x0C)
#define AIC_ALIAS_TMR_CFG(n)		(0x5000 + (n) * 0x80 + 0x10)
#define AIC_ALIAS_TMR_CNT(n)		(0x5000 + (n) * 0x80 + 0x14)
#define AIC_ALIAS_TMR_INT_STAT(n)	(0x5000 + (n) * 0x80 + 0x18)
#define AIC_ALIAS_TMR_STATE_SET(n)	(0x5000 + (n) * 0x80 + 0x1C)
#define AIC_ALIAS_TMR_STATE_CLR(n)	(0x5000 + (n) * 0x80 + 0x20)

#define AIC_EXT_INT_SHIFT		(5)
#define AIC_EXT_INT_MASK		(0x1F)

#define AIC_IO_BASE	(0x0)
#define AIC_IO_SIZE	(0x6000)

struct aic_vdev {
	struct vdev vdev;
};

int aic_xlate_irq(struct device_node *node,
		uint32_t *intspec, unsigned int intsize,
		uint32_t *hwirq, unsigned long *type)
{
	return 0;
}

static int aic_generate_virq(uint32_t *addr, int virq)
{
	return 0;
}

static int aic_update_virq(struct vcpu *vcpu,
		struct virq_desc *desc, int action)
{
	return 0;
}

static int aic_enter_to_guest(struct vcpu *vcpu, void *data)
{
	return 0;
}

static void aic_virq_deinit(struct vdev *vdev)
{
	vdev_release(vdev);
}

static void aic_virq_reset(struct vdev *vdev)
{

}

static int aic_virq_read(struct vdev *vdev, gp_regs *reg,
		unsigned long address, unsigned long *read_value)
{
	return 0;
}

static int aic_virq_write(struct vdev *vdev, gp_regs *reg,
		unsigned long address, unsigned long *value)
{
	return 0;
}

struct virq_chip *create_aic_virqchip(struct vm *vm,
		unsigned long base, unsigned long size)
{
	struct aic_vdev *aic;
	struct virq_chip *vc;

	aic = zalloc(sizeof(struct aic_vdev));
	if (!aic)
		return NULL;

	host_vdev_init(vm, &aic->vdev, base, size);
	vdev_set_name(&aic->vdev, "apple-aic");
	aic->vdev.read = aic_virq_read;
	aic->vdev.write = aic_virq_write;
	aic->vdev.reset = aic_virq_reset;
	aic->vdev.deinit = aic_virq_deinit;

	vc = alloc_virq_chip();
	if (!vc) {
		return NULL;
	}

	vc->xlate = aic_xlate_irq;
	vc->exit_from_guest = NULL;
	vc->enter_to_guest = aic_enter_to_guest;
	vc->generate_virq = aic_generate_virq;
	vc->update_virq = aic_update_virq;
	vc->inc_pdata = NULL;

	return NULL;
}

static struct virq_chip *aic_virqchip_init(struct vm *vm,
		struct device_node *node)
{
	return create_aic_virqchip(vm);
}
VIRQCHIP_DECLARE(vaic_virqchip, aic_match_table, aic_virqchip_init);
