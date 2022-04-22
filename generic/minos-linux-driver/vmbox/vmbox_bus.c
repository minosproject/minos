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
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>
#include <linux/of_irq.h>
#include <asm/io.h>
#include "../minos.h"
#include "vmbox_bus.h"

static ssize_t device_show(struct device *d,
			   struct device_attribute *attr, char *buf)
{
	struct vmbox_device *dev = to_vmbox_device(d);

	return sprintf(buf, "0x%04x\n", dev->id.device);
}
static DEVICE_ATTR_RO(device);

static ssize_t vendor_show(struct device *d,
			   struct device_attribute *attr, char *buf)
{
	struct vmbox_device *dev = to_vmbox_device(d);

	return sprintf(buf, "0x%04x\n", dev->id.vendor);
}
static DEVICE_ATTR_RO(vendor);

static ssize_t modalias_show(struct device *d,
			     struct device_attribute *attr, char *buf)
{
	struct vmbox_device *dev = to_vmbox_device(d);

	return sprintf(buf, "vmbox:d%08Xv%08X\n",
		       dev->id.device, dev->id.vendor);
}
static DEVICE_ATTR_RO(modalias);

static struct attribute *vmbox_dev_attrs[] = {
	&dev_attr_device.attr,
	&dev_attr_vendor.attr,
	&dev_attr_modalias.attr,
	NULL,
};
ATTRIBUTE_GROUPS(vmbox_dev);

static inline int vmbox_id_match(const struct vmbox_device *dev,
			       const struct vmbox_device_id *id)
{
	if (id->device != dev->id.device && id->device != VMBOX_ANY_DEV_ID)
		return 0;

	return id->vendor == VMBOX_ANY_VENDOR_ID || id->vendor == dev->id.vendor;
}

static int vmbox_dev_match(struct device *dv, struct device_driver *dr)
{
	unsigned int i;
	struct vmbox_device *dev = to_vmbox_device(dv);
	const struct vmbox_device_id *ids;

	ids = to_vmbox_driver(dr)->id_table;
	for (i = 0; ids[i].device; i++)
		if (vmbox_id_match(dev, &ids[i]))
			return 1;
	return 0;
}

static int vmbox_uevent(struct device *dv, struct kobj_uevent_env *env)
{
	struct vmbox_device *dev = to_vmbox_device(dv);

	return add_uevent_var(env, "MODALIAS=vmbox:d%08Xv%08X",
			      dev->id.device, dev->id.vendor);
}

static int vmbox_dev_probe(struct device *d)
{
	struct vmbox_device *dev = to_vmbox_device(d);
	struct vmbox_driver *drv = to_vmbox_driver(dev->dev.driver);

	return drv->probe(dev);
}

static int vmbox_dev_remove(struct device *d)
{
	struct vmbox_device *dev = to_vmbox_device(d);
	struct vmbox_driver *drv = to_vmbox_driver(dev->dev.driver);

	drv->remove(dev);
	return 0;
}

static struct bus_type vmbox_bus = {
	.name = "vmbox_bus",
	.match = vmbox_dev_match,
	.dev_groups = vmbox_dev_groups,
	.uevent = vmbox_uevent,
	.probe = vmbox_dev_probe,
	.remove = vmbox_dev_remove,
};

static void vmbox_release_dev(struct device *d)
{
	struct vmbox_device *dev = to_vmbox_device(d);

	/*
	 * free the resouce which this device get include the
	 * memory for virtqueue and others TBD
	 */
	kfree(dev);
}

int vmbox_register_device(struct vmbox_device *vdev)
{
	/* register this vmbox device */
	vdev->dev.bus = &vmbox_bus;
	vdev->dev.release = vmbox_release_dev;
	vdev->dev.init_name = NULL;
	dev_set_name(&vdev->dev, "vmbox-device-%d", vdev->index);

	return device_add(&vdev->dev);
}
EXPORT_SYMBOL_GPL(vmbox_register_device);

void vmbox_unregister_device(struct vmbox_device *dev)
{
	device_unregister(&dev->dev);
}
EXPORT_SYMBOL_GPL(vmbox_unregister_device);

int vmbox_register_driver(struct vmbox_driver *driver)
{
	if (driver->driver.name)
		pr_info("register vmbox driver: %s...\n",
				driver->driver.name);

	driver->driver.bus = &vmbox_bus;
	return driver_register(&driver->driver);
}
EXPORT_SYMBOL_GPL(vmbox_register_driver);

void vmbox_unregister_driver(struct vmbox_driver *drv)
{
	driver_unregister(&drv->driver);
}
EXPORT_SYMBOL_GPL(vmbox_unregister_driver);

void *vmbox_device_remap(struct vmbox_device *vdev)
{
	void *base = NULL;

	if (vdev->vring_va) {
		base = vdev->vring_va;
		goto out;
	}

	base = ioremap_wc((unsigned long)vdev->vring_pa,
				vdev->vring_mem_size);
	if (!base) {
		pr_err("remap vmbox device memory failed\n");
		return NULL;
	}

	vdev->vring_va = base;
out:
	vdev->data_base = base + VMBOX_IPC_ALL_ENTRY_SIZE;
	if (vmbox_device_is_backend(vdev)) {
		vdev->ipc_out = (struct vmbox_ipc_entry *)base;
		vdev->ipc_in = (struct vmbox_ipc_entry *)(base +
				VMBOX_IPC_PER_ENTRY_SZIE);
	} else {
		vdev->ipc_in = (struct vmbox_ipc_entry *)base;
		vdev->ipc_out = (struct vmbox_ipc_entry *)(base +
				VMBOX_IPC_PER_ENTRY_SZIE);
	}

	return vdev->data_base;
}

void vmbox_device_unmap(struct vmbox_device *vdev)
{
	if (vdev->vring_va)
		iounmap(vdev->vring_va);
}

static void vmbox_release_virtq(struct vmbox_device *vdev)
{
	int i;

	if (vdev->vqs) {
		for (i = 0; i < vdev->nr_vqs; i++) {
			if (vdev->vqs[i])
				kfree(vdev->vqs[i]);
		}
		kfree(vdev->vqs);
	}
}

static void vmbox_ipc_event_work(struct work_struct *work)
{
	int event, bit;
	struct vmbox_device *vdev = container_of(work, struct vmbox_device, ws);
	struct vmbox_driver *drv = to_vmbox_driver(vdev->dev.driver);

	if (!vmbox_device_state(vdev)) {
		event = readl(vdev->iomem + VMBOX_DEV_IPC_TYPE);
		dev_err(&vdev->dev, "device is not online 0x%x\n", event);
		return;
	}

	event = readl(vdev->iomem + VMBOX_DEV_IPC_TYPE);

	while (event != 0) {
		bit = ffs(event) - 1;
		if (bit == VMBOX_IPC_STATE_CHANGE) {
			if (drv->otherside_state_change)
				drv->otherside_state_change(vdev,
					vmbox_device_otherside_state(vdev));
		} else {
			if (drv->otherside_evt_handler)
				drv->otherside_evt_handler(vdev, bit);
		}
		event &= ~(1 << bit);
	}
}

int vmbox_device_init(struct vmbox_device *vdev, unsigned long flags)
{
	int i;
	struct vmbox_virtqueue *vq;
	struct vmbox_driver *vdrv = to_vmbox_driver(vdev->dev.driver);

	INIT_WORK(&vdev->ws, vmbox_ipc_event_work);
	vdev->workqueue = create_singlethread_workqueue(dev_name(&vdev->dev));
	if (!vdev->workqueue) {
		pr_err("unable create workqueue for vmbox device\n");
		return -EBUSY;
	}

	vdev->flags |= flags;
	if (vdev->flags & VMBOX_F_NO_VIRTQ) {
		pr_info("vmbox device using no virtq mode %s\n",
				dev_name(&vdev->dev));
		return 0;
	}

	if (!vdev->vring_va)
		vmbox_device_remap(vdev);

	pr_info("%s: nr_vqs: %d\n", __func__, vdev->nr_vqs);

	if (vdev->nr_vqs) {
		vdev->vqs = kzalloc(sizeof(struct vmbox_virtqueue *) *
				vdev->nr_vqs, GFP_KERNEL);
		if (NULL == vdev->vqs)
			goto release_vqs;
	}

	/* init the all vqs fot this vmbox_device */
	for (i = 0; i < vdev->nr_vqs; i++) {
		vq = kzalloc(sizeof(*vq), GFP_KERNEL);
		if (!vq)
			goto release_vqs;

		vmbox_virtq_init(vdev, vq, i);
		vdev->vqs[i] = vq;
	}

	if (vdrv->setup_vq)
		vdrv->setup_vq(vdev);
	else
		dev_warn(&vdev->dev, "no virtqueue setup function\n");

	return 0;

release_vqs:
	pr_err("can not alloc memory for vqs\n");
	destroy_workqueue(vdev->workqueue);
	vmbox_release_virtq(vdev);
	return -ENOMEM;
}

static irqreturn_t vmbox_vring_irq_handler(int irq, void *dev_id)
{
	int i;
	struct vmbox_virtqueue *vq;
	struct vmbox_device *vdev = (struct vmbox_device *)dev_id;

	for (i = 0; i < vdev->nr_vqs; i++) {
		vq = vdev->vqs[i];

		if (vq && (!vq->broken) && vq->callback &&
				(vq->status == VMBOX_VIRTQ_STARTED))
			vq->callback(vq);
		else
			/* all the data need to be droped */
			vmbox_virtq_consume_descs(vq);
	}

	return IRQ_HANDLED;
}

static irqreturn_t vmbox_ipc_irq_handler(int irq, void *dev_id)
{
	struct vmbox_device *vdev = (struct vmbox_device *)dev_id;

	queue_work(vdev->workqueue, &vdev->ws);
	return IRQ_HANDLED;
}

int vmbox_device_vring_event(struct vmbox_device *vdev)
{
	if (!vmbox_device_otherside_open(vdev)) {
		pr_debug("other side is not opened\n");
		return -EINVAL;
	}

	writel(1, vdev->iomem + VMBOX_DEV_VRING_EVENT);
	return 0;
}

int vmbox_device_ipc_event(struct vmbox_device *vdev, int event)
{
	if (event >= VMBOX_DEV_IPC_COUNT)
		return -EINVAL;

	writel(event, vdev->iomem + VMBOX_DEV_IPC_EVENT);
	return 0;
}

void vmbox_device_state_change(struct vmbox_device *vdev, int state)
{
	vdev->ipc_out->state = state;
	mb();
	vmbox_device_ipc_event(vdev, VMBOX_IPC_STATE_CHANGE);
}

static void inline __vmbox_device_online(struct vmbox_device *vdev)
{
	/*
	 * for online event, if the device is backedn, then
	 * it will write its ONLINE register to inform forent
	 * side to online, otherwise, it will send a virq to
	 * the backend
	 */
	vmbox_device_state(vdev) = VMBOX_DEV_STAT_ONLINE;
	writel(1, vdev->iomem + VMBOX_DEV_VDEV_ONLINE);
}

int vmbox_device_online(struct vmbox_device *vdev)
{
	int ret;

	/*
	 * create a workqueue to handle the ipc event to
	 * avoid deadlock
	 */

	ret = request_irq(vdev->event_irq, vmbox_ipc_irq_handler,
			IRQF_NO_SUSPEND,
			dev_name(&vdev->dev), vdev);
	if (ret) {
		dev_err(&vdev->dev, "request event IRQ: %d, error: %d\n",
				vdev->event_irq, ret);
		return ret;
	}

	/*
	 * only request the vring irq if this vmbox device
	 * using virtq to transfer data between VMs
	 */
	if (!(vdev->flags & VMBOX_F_NO_VIRTQ) && !(vdev->flags &
				VMBOX_F_VRING_IRQ_MANUAL)) {
		ret = request_irq(vdev->vring_irq, vmbox_vring_irq_handler,
				IRQF_NO_SUSPEND,
				dev_name(&vdev->dev), vdev);
		if (ret) {
			dev_err(&vdev->dev,
				"failed to request vring IRQ: %d, error: %d\n",
				vdev->event_irq, ret);
			return ret;
		}
	}

	__vmbox_device_online(vdev);

	return ret;
}

static int __init vmbox_bus_init(void)
{
	pr_info("register vmbox bus\n");
	return bus_register(&vmbox_bus);
}

static void __exit vmbox_bus_exit(void)
{
	bus_unregister(&vmbox_bus);
}

core_initcall(vmbox_bus_init);
module_exit(vmbox_bus_exit);

MODULE_AUTHOR("Min Le lemin9538@gmail.com");
MODULE_LICENSE("GPL v2");
