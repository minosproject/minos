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
#include <minos/vdev.h>
#include <asm/io.h>
#include <minos/irq.h>
#include <minos/timer.h>
#include <minos/gvm.h>
#include <minos/time.h>

#define SP805_WDT_LOAD		0x00
#define SP805_WDT_VALUE		0x04
#define SP805_WDT_CTL		0x08
#define SP805_WDT_INTCLR	0x0c
#define SP805_WDT_RIS		0x10
#define SP805_WDT_MIS		0x14
#define SP805_WDT_LOCK		0xc00
#define SP805_WDT_ITCR		0xf00
#define SP805_WDT_ITOP		0xf04
#define SP805_WDT_PRID0		0xfe0
#define SP805_WDT_PRID1		0xfe4
#define SP805_WDT_PRID2		0xfe8
#define SP805_WDT_PRID3		0xfec
#define SP805_WDT_PCID0		0xff0
#define SP805_WDT_PCID1		0xff4
#define SP805_WDT_PCID2		0xff8
#define SP805_WDT_PCID3		0xffc

#define LOAD_MIN		0x00000001
#define LOAD_MAX		0xffffffff
#define INT_ENABLE		(1 << 0)
#define RESET_ENABLE		(1 << 1)
#define INT_MASK		(1 << 0)
#define WDT_LOCK		0x00000001
#define WDT_UNLOCK		0x1acce551
#define WDT_CLK_RATE		32768

struct vwdt_dev {
	uint8_t int_enable;
	uint8_t reset_enable;
	uint8_t int_trigger;
	uint8_t access_lock;
	unsigned long load_value;
	unsigned long timeout_value;
	struct timer_list wdt_timer;
	struct vdev vdev;
};

#define vdev_to_vwdt(vdev) \
	(struct vwdt_dev *)container_of(vdev, struct vwdt_dev, vdev)

static uint32_t inline wdt_timeleft(struct vwdt_dev *wdt)
{
	unsigned long left = wdt->timeout_value - NOW();

	/* convert the time to wdt clk rate ticks */
	return (left / 1000000000) * WDT_CLK_RATE;
}

static int vwdt_mmio_read(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *value)
{
	unsigned long offset = address - SP805_IOMEM_BASE;
	struct vwdt_dev *wdt = vdev_to_vwdt(vdev);

	switch (offset) {
	case SP805_WDT_VALUE:
		*value = (uint32_t)wdt_timeleft(wdt);
		break;
	case SP805_WDT_CTL:
		*value = wdt->reset_enable << 1 | wdt->int_enable;
		break;
	case SP805_WDT_LOCK:
		*value = wdt->access_lock;
		break;
	case SP805_WDT_RIS:
		*value = wdt->int_trigger;
		break;
	case SP805_WDT_MIS:
		*value = wdt->int_enable;
		break;
	case SP805_WDT_PRID0:
		*value = 0x5;
		break;
	case SP805_WDT_PRID1:
		*value = (0x1 << 4) | 0x8;
		break;
	case SP805_WDT_PRID2:
		*value = (0x1 << 4) | 0x4;
		break;
	case SP805_WDT_PRID3:
		*value = 0x0;
		break;
	case SP805_WDT_PCID0:
		*value = 0x0d;
		break;
	case SP805_WDT_PCID1:
		*value = 0xf0;
		break;
	case SP805_WDT_PCID2:
		*value = 0x05;
		break;
	case SP805_WDT_PCID3:
		*value = 0xb1;
		break;
	}

	return 0;
}

static void vwdt_timer_expire(unsigned long data)
{
	struct vwdt_dev *wdt = (struct vwdt_dev *)data;

	/* if the timeout int has already triggered reset
	 * the system if the reset is enabled */
	if (wdt->int_trigger && wdt->reset_enable) {
		if (vm_is_native(wdt->vdev.vm))
			panic("native vm watchdog timeout\n");
		else
			trap_vcpu_nonblock(VMTRAP_TYPE_COMMON,
					VMTRAP_REASON_WDT_TIMEOUT, 0, NULL);
	} else {
		if (wdt->int_enable) {
			send_virq_to_vm(wdt->vdev.vm, SP805_IRQ);
			wdt->int_trigger = 1;
			wdt->timeout_value = NOW() + wdt->load_value;
			mod_timer(&wdt->wdt_timer, wdt->timeout_value);
		}
	}
}

static void inline vwdt_config_ctrl(struct vwdt_dev *wdt,
		int int_enable, int reset_enable)
{
	if (wdt->int_enable != int_enable) {
		wdt->int_enable = int_enable;

		if (int_enable) {
			wdt->timeout_value = NOW() + wdt->load_value;
			mod_timer(&wdt->wdt_timer, wdt->timeout_value);
		} else {
			wdt->int_trigger = 0;
			del_timer(&wdt->wdt_timer);
		}
	}

	if (wdt->reset_enable != reset_enable)
		wdt->reset_enable = reset_enable;
}

static void inline vwdt_config_intclr(struct vwdt_dev *wdt)
{
	wdt->int_trigger = 0;

	/* reload the value and restart the timer */
	if (wdt->int_enable) {
		wdt->timeout_value = NOW() + wdt->load_value;
		mod_timer(&wdt->wdt_timer, wdt->timeout_value);
	}
}

static void inline vwdt_config_load(struct vwdt_dev *wdt, uint32_t v)
{
	unsigned long timeout = 0;

	timeout = (v + WDT_CLK_RATE - 1) / WDT_CLK_RATE;
	wdt->load_value = SECONDS(timeout);
	pr_info("set the timeout value to 0x%x 0x%x\n",
			timeout, wdt->load_value);
}

static int vwdt_mmio_write(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *value)
{
	uint32_t v = (uint32_t)(*value);
	struct vwdt_dev *wdt = vdev_to_vwdt(vdev);
	unsigned long offset = address - SP805_IOMEM_BASE;

	if ((offset != SP805_WDT_LOCK) && wdt->access_lock) {
		pr_error("register is locked of the wdt\n");
		return -EPERM;
	}

	switch (offset) {
	case SP805_WDT_LOAD:
		vwdt_config_load(wdt, v);
		break;
	case SP805_WDT_CTL:
		vwdt_config_ctrl(wdt, !!(v & INT_ENABLE),
				!!(v & RESET_ENABLE));
		break;
	case SP805_WDT_INTCLR:
		vwdt_config_intclr(wdt);
		break;
	case SP805_WDT_LOCK:
		if (v == WDT_UNLOCK)
			wdt->access_lock = 0;
		else if (v == WDT_LOCK)
			wdt->access_lock = 1;
		else
			pr_warn("unsupport value of wdt_lock\n");
		break;
	default:
		break;
	}

	return 0;
}

static void vwdt_reset(struct vdev *vdev)
{
	struct vwdt_dev *dev = vdev_to_vwdt(vdev);

	pr_info("vwdt reset\n");

	dev->int_enable = 0;
	dev->access_lock = 1;
	dev->reset_enable = 0;
	dev->int_trigger = 0;
	dev->load_value = 0;
	dev->timeout_value = 0;
	del_timer(&dev->wdt_timer);
}

static void vwdt_deinit(struct vdev *vdev)
{
	struct vwdt_dev *dev = vdev_to_vwdt(vdev);

	del_timer(&dev->wdt_timer);
	dev->int_enable = 0;
	dev->access_lock = 1;
	dev->reset_enable = 0;
	dev->int_trigger = 0;

	vdev_release(&dev->vdev);
	free(dev);
}

static int vwdt_create_vm(void *item, void *arg)
{
	struct vm *vm = (struct vm *)item;
	struct vwdt_dev *dev;
	struct vcpu *vcpu = get_vcpu_in_vm(vm, 0);

	if (vm_is_hvm(vm))
		return 0;

	pr_info("create virtual watchdog for vm-%d\n", vm->vmid);

	dev = zalloc(sizeof(struct vwdt_dev));
	if (!dev)
		return -ENOMEM;

	host_vdev_init(vm, &dev->vdev, SP805_IOMEM_BASE,
			SP805_IOMEM_SIZE);
	vdev_set_name(&dev->vdev, "vwdt");
	virq_mask_and_enable(vm, SP805_IRQ, 0);

	dev->access_lock = 1;
	dev->vdev.read = vwdt_mmio_read;
	dev->vdev.write = vwdt_mmio_write;
	dev->vdev.deinit = vwdt_deinit;
	dev->vdev.reset = vwdt_reset;

	init_timer_on_cpu(&dev->wdt_timer, vcpu->affinity);
	dev->wdt_timer.function = vwdt_timer_expire;
	dev->wdt_timer.data = (unsigned long)dev;

	return 0;
}

int vwdt_init(void)
{
	return register_hook(vwdt_create_vm,
			MINOS_HOOK_TYPE_CREATE_VM_VDEV);
}

module_initcall(vwdt_init);
