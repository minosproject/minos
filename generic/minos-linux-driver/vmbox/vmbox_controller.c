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
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/of_irq.h>
#include <asm/io.h>
#include <linux/kthread.h>
#include "vmbox_bus.h"

#include "../minos.h"

struct vmbox_controller {
	void *con_iomem;
	void *device_iomem;
	int irq;
	uint32_t device_state;
	struct platform_device *pdev;
	struct task_struct *hotplug_thread;
	wait_queue_head_t hotplug_wait;
	char hotplug_thread_name[32];
};

static struct vmbox_controller *vcon;

static int vmbox_get_device_info(struct vmbox_device *vdev)
{
	unsigned long high, low;

	vdev->nr_vqs = readl(vdev->iomem  + VMBOX_DEV_VQS);
	vdev->vring_num = readl(vdev->iomem + VMBOX_DEV_VRING_NUM);
	vdev->vring_size = readl(vdev->iomem + VMBOX_DEV_VRING_SIZE);
	vdev->vring_irq = readl(vdev->iomem + VMBOX_DEV_VRING_IRQ);
	vdev->event_irq = readl(vdev->iomem + VMBOX_DEV_IPC_IRQ);
	vdev->vring_mem_size = readl(vdev->iomem + VMBOX_DEV_MEM_SIZE);
	high = readl(vdev->iomem + VMBOX_DEV_VRING_BASE_HI);
	low = readl(vdev->iomem + VMBOX_DEV_VRING_BASE_LOW);
#ifdef CONFIG_64BIT
	vdev->vring_pa = (void *)((high << 32) | low);
#else
	vdev->vring_pa = (void *)low;
#endif
	vdev->id.device = readl(vdev->iomem + VMBOX_DEV_DEVICE_ID);
	vdev->id.vendor = readl(vdev->iomem + VMBOX_DEV_VENDOR_ID);

	vdev->event_irq = get_dynamic_virq(vdev->event_irq);
	vdev->vring_irq = get_dynamic_virq(vdev->vring_irq);

	/*
	 * if the device id is not 2N then this device is a backend
	 * device which will service the frontend device
	 */
	if ((vdev->id.device % 2) == 0)
		vdev->flags |= VMBOX_F_DEV_BACKEND;

	if (!vdev->vring_pa || !vdev->vring_mem_size)
		return -ENOMEM;

	return 0;
}

static int vmbox_device_add(struct vmbox_controller *vcon, int index)
{
	uint32_t value;
	struct vmbox_device *vdev;
	void *dev_reg;

	if (index >= 32)
		return -EINVAL;

	vdev = kzalloc(sizeof(*vdev), GFP_KERNEL);
	if (!vdev)
		return -ENOMEM;

	dev_reg = vcon->con_iomem + VMBOX_CON_DEV_BASE +
		index * VMBOX_CON_DEV_SIZE;
	value = readl(dev_reg + VMBOX_DEV_ID);
	if (((value & 0xffffff00) != VMBOX_DEVICE_MAGIC) ||
			((value & 0xff) != index)) {
		pr_err("wrong vmbox device id\n");
		return -ENOENT;
	}

	vdev->iomem = dev_reg;
	vdev->index = index;
	if (vmbox_get_device_info(vdev))
		return -EINVAL;

	pr_info("found new vmbox device dev: 0x%x vendor: 0x%x\n",
			vdev->id.device, vdev->id.vendor);

	/* register this vmbox device */
	device_initialize(&vdev->dev);
	vdev->dev.parent = &vcon->pdev->dev;

	return vmbox_register_device(vdev);
}

static void vmbox_device_hotplug(struct vmbox_controller *vcon)
{
	int i, ret;
	uint32_t old_state = vcon->device_state;
	uint32_t new_state = readl(vcon->con_iomem + VMBOX_CON_DEV_STAT);

	pr_info("vmbox devic state changed old: 0x%x new: 0x%x\n",
			old_state, new_state);

	if (new_state == old_state)
		return;

	/* currently do not support unplug the device */
	vcon->device_state = new_state;
	new_state &= ~old_state;
	for (i = 0; i < 32; i++) {
		if (new_state & (1 << i)) {
			ret = vmbox_device_add(vcon, i);
			if (ret)
				pr_err("vmbox-dev-%d online failed\n", i);
		}
	}
}

static int vmbox_hotplug_thread(void *data)
{
	int ret;
	uint32_t new_state;
	struct vmbox_controller *vcon = platform_get_drvdata(data);

	do {
		new_state = readl(vcon->con_iomem + VMBOX_CON_DEV_STAT);
		if (new_state != vcon->device_state) {
			vmbox_device_hotplug(vcon);
		} else {
			/* wait the signal timeout 2S */
			ret = wait_event_interruptible_timeout(
					vcon->hotplug_wait,
					kthread_should_stop(), HZ);
			if (ret < 0) {
				pr_err("wait hotplug failed\n");
				break;
			}
		}

	} while (!kthread_should_stop());

	return 0;
}

static irqreturn_t vmbox_controller_int(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct vmbox_controller *vcon = platform_get_drvdata(pdev);
	uint32_t value;

	value = readl(vcon->con_iomem + VMBOX_CON_INT_STATUS);
	pr_info("%s value is 0x%x\n", __func__, value);
	if (value & VMBOX_CON_INT_TYPE_DEV_ONLINE)
		wake_up(&vcon->hotplug_wait);
	writel(value, vcon->con_iomem + VMBOX_CON_INT_STATUS);

	return 0;
}

static int vmbox_platform_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *mem;
	struct device_node *np = pdev->dev.of_node;

	if (vcon) {
		dev_err(&pdev->dev, "vmbox controller already register\n");
		return -EEXIST;
	}

	vcon = kzalloc(sizeof(struct vmbox_controller), GFP_KERNEL);
	if (!vcon)
		return -ENOMEM;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "no memory region found\n");
		goto release_vcon;
	}

	vcon->pdev = pdev;
	vcon->con_iomem = devm_ioremap_resource(&pdev->dev, mem);
	if (IS_ERR(vcon->con_iomem))
		return PTR_ERR(vcon->con_iomem);
	vcon->device_iomem = vcon->con_iomem + VMBOX_CON_DEV_BASE;

	vcon->irq = irq_of_parse_and_map(np, 0);
	if (!vcon->irq) {
		dev_err(&pdev->dev, "can not find irq for device\n");
		goto unmap_mem;
	}

	platform_set_drvdata(pdev, vcon);

	/*
	 * create the kernel thread for this controller to handle
	 * the device hotplug
	 */
	strcpy(vcon->hotplug_thread_name, "vmbox-dev-hotplug");
	init_waitqueue_head(&vcon->hotplug_wait);
	vcon->hotplug_thread = kthread_create(vmbox_hotplug_thread,
			pdev, vcon->hotplug_thread_name);
	if (!vcon->hotplug_thread) {
		dev_err(&pdev->dev, "create hotplug thread failed\n");
		goto unmap_mem;
	}

	/* register the irq for this controller */
	ret = request_irq(vcon->irq, vmbox_controller_int,
			IRQF_NO_SUSPEND, dev_name(&pdev->dev), pdev);
	if (ret) {
		dev_err(&pdev->dev, "can not register irq\n");
		goto unmap_mem;
	}

	/* set the VCON online */
	writel(1, vcon->con_iomem + VMBOX_CON_ONLINE);

	wake_up_process(vcon->hotplug_thread);

	return 0;

unmap_mem:
	devm_iounmap(&pdev->dev, vcon->con_iomem);
release_vcon:
	kfree(vcon);
	return -EINVAL;;
}

static int vmbox_platform_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id vmbox_platform_dt_ids[] = {
	{.compatible = "minos,vmbox"},
	{}
};

static struct platform_driver vmbox_platform_driver = {
	.probe = vmbox_platform_probe,
	.remove = vmbox_platform_remove,
	.driver = {
		.name = "vmbox-controller",
		.of_match_table = vmbox_platform_dt_ids,
	},
};

static __init int vmbox_controller_init(void)
{
	pr_info("register vmbox controller driver\n");
	return platform_driver_register(&vmbox_platform_driver);
}

static void __exit vmbox_controller_exit(void)
{
	platform_driver_unregister(&vmbox_platform_driver);
}

module_init(vmbox_controller_init);
module_exit(vmbox_controller_exit);
MODULE_LICENSE("GPL");
