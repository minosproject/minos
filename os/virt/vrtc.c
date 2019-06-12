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
#include <common/gvm.h>
#include <minos/time.h>
#include <minos/resource.h>

#define	RTC_DR		0x00		/* Data read register */
#define	RTC_MR		0x04		/* Match register */
#define	RTC_LR		0x08		/* Data load register */
#define	RTC_CR		0x0c		/* Control register */
#define	RTC_IMSC	0x10		/* Interrupt mask and set register */
#define	RTC_RIS		0x14		/* Raw interrupt status register */
#define	RTC_MIS		0x18		/* Masked interrupt status register */
#define	RTC_ICR		0x1c		/* Interrupt clear register */
#define RTC_PID0	0xfe0
#define RTC_PID1	0xfe4
#define RTC_PID2	0xfe8
#define RTC_PID3	0xfec
#define RTC_PCID0	0xff0
#define RTC_PCID1	0xff4
#define RTC_PCID2	0xff8
#define RTC_PCID3	0xffc

#define RTC_CR_EN	(1 << 0)	/* counter enable bit */
#define RTC_BIT_AI	(1 << 0)	/* Alarm interrupt bit */

#define RTC_TIMER_FREQ 32768

struct vrtc_dev {
	struct vdev vdev;
	int rtc_en;
	int rtc_int_en;
	int rtc_int_trigger;
	uint32_t time_base;
	uint32_t time_alarm;
	uint64_t time_offset;
	struct timer_list alarm_timer;
};

#define vdev_to_vrtc(vdev) \
	(struct vrtc_dev *)container_of(vdev, struct vrtc_dev, vdev)

static int vrtc_mmio_read(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *value)
{
	struct vrtc_dev *vrtc = vdev_to_vrtc(vdev);
	unsigned long offset = address - PL031_IOMEM_BASE;
	uint64_t t;

	switch (offset) {
	case RTC_DR:
		if (!vrtc->rtc_en)
			*value = 0;
		else {
			t = NOW() - vrtc->time_offset;
			*value = vrtc->time_base + muldiv64(t, 1, SECONDS(1));
		}
		break;
	case RTC_MR:
		/* read the alarm */
		*value = vrtc->time_alarm;
		break;
	case RTC_LR:
		*value = vrtc->time_base;
		break;
	case RTC_CR:
		*value = vrtc->rtc_en;
		break;
	case RTC_IMSC:
		*value = vrtc->rtc_int_en;
		break;
	case RTC_RIS:
		*value = vrtc->rtc_int_trigger;
		break;
	case RTC_MIS:
		*value = !vrtc->rtc_int_en;
		break;
	case RTC_PID0:
		*value = 0x31;
		break;
	case RTC_PID1:
		*value = 0x10;
		break;
	case RTC_PID2:
		*value = 0x04;
		break;
	case RTC_PID3:
		*value = 0x00;
		break;
	case RTC_PCID0:
		*value = 0x0d;
		break;
	case RTC_PCID1:
		*value = 0xf0;
		break;
	case RTC_PCID2:
		*value = 0x05;
		break;
	case RTC_PCID3:
		*value = 0xb1;
		break;
	}

	return 0;
}

static int vrtc_enable(struct vrtc_dev *vrtc, int en)
{
	unsigned long t;

	if (vrtc->rtc_en == en)
		return 0;

	if (en) {
		/*
		 * should set it to current time or using
		 * default time 1970-0-0-0 ?
		 */
		trap_vcpu(VMTRAP_TYPE_COMMON, VMTRAP_REASON_GET_TIME, 0, &t);
		vrtc->time_base = t;
		vrtc->time_offset = NOW();
	} else {
		/* delete the alarm if started */
		del_timer(&vrtc->alarm_timer);
		vrtc->rtc_int_trigger = 0;
		vrtc->rtc_int_en = 0;
		vrtc->time_alarm = 0;
		vrtc->time_base = 0;
		vrtc->time_offset = 0;
	}

	vrtc->rtc_en = en;
	return 0;
}

static int vrtc_int_enable(struct vrtc_dev *vrtc, int mask)
{
	if (vrtc->rtc_int_en != mask)
		return 0;

	/*
	 * if the irq has been trigger and the int
	 * is unmask, then send the virq to the guest
	 */
	if (!mask) {
		if (vrtc->rtc_int_trigger)
			send_virq_to_vm(vrtc->vdev.vm, PL031_IRQ);
	}

	vrtc->rtc_int_en = !mask;
	return 0;
}

static int vrtc_set_alarm(struct vrtc_dev *vrtc, uint32_t alarm)
{
	uint64_t timeout;

	if (alarm <= vrtc->time_base)
		return -EINVAL;

	timeout = SECONDS(alarm - vrtc->time_base) + vrtc->time_offset;
	mod_timer(&vrtc->alarm_timer, timeout);

	return 0;
}

static int vrtc_mmio_write(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *value)
{
	uint32_t v = *((uint32_t *)value);
	struct vrtc_dev *vrtc = vdev_to_vrtc(vdev);
	unsigned long offset = address - PL031_IOMEM_BASE;

	switch (offset) {
	case RTC_MR:
		/* set the alarm */
		vrtc_set_alarm(vrtc, v);
		break;
	case RTC_LR:
		vrtc->time_offset = NOW();
		vrtc->time_base = v;
		break;
	case RTC_CR:
		vrtc_enable(vrtc, v);
		break;
	case RTC_IMSC:
		vrtc_int_enable(vrtc, v);
		break;
	case RTC_ICR:
		vrtc->rtc_int_trigger = 0;
		break;
	}

	return 0;
}

static void vrtc_alarm_function(unsigned long data)
{
	struct vrtc_dev *vrtc = (struct vrtc_dev *)data;

	vrtc->rtc_int_trigger = 1;
	if (vrtc->rtc_int_en)
		send_virq_to_vm(vrtc->vdev.vm, PL031_IRQ);
}

static void vrtc_reset(struct vdev *vdev)
{
	/* do nothing here */
	pr_info("vrtc reset\n");
}

static void vrtc_deinit(struct vdev *vdev)
{
	struct vrtc_dev *dev = vdev_to_vrtc(vdev);

	del_timer(&dev->alarm_timer);
	dev->rtc_en = 0;
	dev->rtc_int_en = 0;
	dev->rtc_int_trigger = 0;

	vdev_release(&dev->vdev);
	free(dev);
}

static void *vrtc_init(struct vm *vm, struct device_node *node)
{
	int ret;
	uint32_t irq;
	struct vrtc_dev *dev;
	struct vcpu *vcpu = get_vcpu_in_vm(vm, 0);
	uint64_t base, size;
	unsigned long flags;

	pr_info("create virtual rtc for vm-%d\n", vm->vmid);

	ret = translate_device_address(node, &base, &size);
	if (ret || (size == 0))
		return NULL;

	ret = get_device_irq_index(vm, node, &irq, &flags, 0);
	if (ret)
		return NULL;

	dev = zalloc(sizeof(struct vrtc_dev));
	if (!dev)
		return NULL;

	host_vdev_init(vm, &dev->vdev, base, size);
	vdev_set_name(&dev->vdev, "vrtc");
	request_virq(vm, irq, flags | VIRQF_CAN_WAKEUP);

	dev->vdev.read = vrtc_mmio_read;
	dev->vdev.write = vrtc_mmio_write;
	dev->vdev.deinit = vrtc_deinit;
	dev->vdev.reset = vrtc_reset;

	init_timer_on_cpu(&dev->alarm_timer, vcpu->affinity);
	dev->alarm_timer.function = vrtc_alarm_function;
	dev->alarm_timer.data = (unsigned long)dev;

	return (void *)dev;
}
VDEV_DECLARE(pl031_vrtc, pl031_match_table, vrtc_init);
