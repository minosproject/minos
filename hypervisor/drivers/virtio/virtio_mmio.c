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
#include <minos/vmm.h>
#include <minos/mm.h>
#include <minos/bitmap.h>
#include <minos/virtio.h>
#include <minos/io.h>
#include <minos/sched.h>
#include <minos/vdev.h>
#include <minos/virq.h>
#include <minos/virtio_mmio.h>

#define vdev_to_virtio(vd) \
	container_of(vd, struct virtio_device, vdev)

static int virtio_mmio_read(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *read_value)
{
	return 0;
}

static inline void
virtio_notify_hvm(struct virtio_device *dev, int nonblock)
{
	struct vdev *vdev = &dev->vdev;
	struct vcpu * hvm_vcpu0 = get_vcpu_in_vm(get_vm_by_id(0), 0);

	iowrite32(vdev->iomem + VIRTIO_MMIO_EVENT_ACK, 0);

	/*
	 * wait for the host vm handle this message
	 * as default the virq will send the vcpu0
	 * of the hvm, if the virtio's vcpu is as
	 * same as the hvm vcpu0 then sched out otherwise
	 * do loop
	 */
	vdev_notify_hvm(vdev, dev->hvm_irq);

	if (!nonblock) {
		while (!ioread32(vdev->iomem + VIRTIO_MMIO_EVENT_ACK)) {
			if (vcpu_affinity(hvm_vcpu0) ==
					vcpu_affinity(current_vcpu))
				sched();
			else
				cpu_relax();
		}

		iowrite32(vdev->iomem + VIRTIO_MMIO_EVENT_ACK, 0);
	}
}

static int virtio_mmio_write(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *write_value)
{
	uint32_t tmp;
	uint32_t value = *(uint32_t *)write_value;
	void *iomem = vdev->iomem;
	unsigned long offset = address - vdev->gvm_paddr;
	struct virtio_device *dev = vdev_to_virtio(vdev);

	switch (offset) {
	case VIRTIO_MMIO_HOST_FEATURES:
		break;
	case VIRTIO_MMIO_HOST_FEATURES_SEL:
		if (value > 3) {
			pr_warn("invalid features sel value %d\n", value);
			break;
		}
		tmp = ioread32(iomem + VIRTIO_MMIO_HOST_FEATURE0 +
				value * sizeof(uint32_t));
		iowrite32(iomem + VIRTIO_MMIO_HOST_FEATURES, tmp);
		break;
	case VIRTIO_MMIO_GUEST_FEATURES_SEL:
		iowrite32(iomem + VIRTIO_MMIO_GUEST_FEATURES_SEL, value);
		break;
	case VIRTIO_MMIO_GUEST_FEATURES:
		tmp = ioread32(iomem + VIRTIO_MMIO_GUEST_FEATURES_SEL);
		tmp = tmp * sizeof(uint32_t) + VIRTIO_MMIO_DRIVER_FEATURE0;
		iowrite32(iomem + tmp, value);
		break;
	case VIRTIO_MMIO_GUEST_PAGE_SIZE:
		/* version 1 virtio device */
		iowrite32(iomem + VIRTIO_MMIO_GUEST_PAGE_SIZE, value);
		break;
	case VIRTIO_MMIO_QUEUE_SEL:
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_SEL, value);

		/* clear the queue information in the memory */
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_READY, 0);
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_NUM, 0);
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_DESC_LOW, 0);
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_DESC_HIGH, 0);
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_AVAIL_LOW, 0);
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_AVAIL_HIGH, 0);
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_USED_LOW, 0);
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_USED_HIGH, 0);
		break;
	case VIRTIO_MMIO_QUEUE_NUM:
		tmp = ioread32(iomem + VIRTIO_MMIO_QUEUE_NUM_MAX);
		if (value > tmp)
			pr_warn("invalid queue sel %d\n", value);
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_NUM, value);
		break;
	case VIRTIO_MMIO_QUEUE_ALIGN:
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_ALIGN, value);
		break;
	case VIRTIO_MMIO_QUEUE_PFN:
		/* this is for version 1 virtio device */
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_PFN, value);
		break;
	case VIRTIO_MMIO_QUEUE_NOTIFY:
		/*
		 * indicate a queue is ready, need send a
		 * event to hvm ?
		 */
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_NOTIFY, value);
		iowrite32(iomem + VIRTIO_MMIO_EVENT, VIRTIO_EVENT_QUEUE_READY);
		virtio_notify_hvm(dev, 0);
		break;
	case VIRTIO_MMIO_STATUS:
		tmp = ioread32(iomem + VIRTIO_MMIO_STATUS);
		value = value - tmp;
		iowrite32(iomem + VIRTIO_MMIO_STATUS, value);
		iowrite32(iomem + VIRTIO_MMIO_EVENT, VIRTIO_EVENT_STATUS_CHANGE);
		virtio_notify_hvm(dev, 0);
		break;
	case VIRTIO_MMIO_QUEUE_DESC_LOW:
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_DESC_LOW, value);
		break;
	case VIRTIO_MMIO_QUEUE_AVAIL_LOW:
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_AVAIL_LOW, value);
		break;
	case VIRTIO_MMIO_QUEUE_USED_LOW:
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_USED_LOW, value);
		break;
	case VIRTIO_MMIO_QUEUE_DESC_HIGH:
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_DESC_HIGH, value);
		break;
	case VIRTIO_MMIO_QUEUE_USED_HIGH:
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_USED_HIGH, value);
		break;
	case VIRTIO_MMIO_QUEUE_AVAIL_HIGH:
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_AVAIL_HIGH, value);
		break;
	case VIRTIO_MMIO_QUEUE_READY:
		iowrite32(iomem + VIRTIO_MMIO_QUEUE_READY, value);
		iowrite32(iomem + VIRTIO_MMIO_EVENT, VIRTIO_EVENT_BUFFER_READY);
		virtio_notify_hvm(dev, 0);
		break;
	case VIRTIO_MMIO_INTERRUPT_ACK:
		iowrite32(iomem + VIRTIO_MMIO_INTERRUPT_ACK, 0);
		iowrite32(iomem + VIRTIO_MMIO_INTERRUPT_STATUS, 0);
		break;
	default:
		break;
	}

	dsb();

	return 0;
}

static inline void virtio_device_init(struct vm *vm,
		struct virtio_device *dev)
{
	void *base = dev->vdev.iomem;

	iowrite32(base + VIRTIO_MMIO_GVM_ADDR, dev->vdev.gvm_paddr);
	iowrite32(base + VIRTIO_MMIO_MEM_SIZE, dev->vdev.mem_size);
	iowrite32(base + VIRTIO_MMIO_HVM_IRQ, dev->hvm_irq);
	iowrite32(base + VIRTIO_MMIO_GVM_IRQ, dev->gvm_irq);
}

void release_virtio_dev(struct vm *vm, struct virtio_device *dev)
{
	if (!dev)
		return;

	vdev_release(&dev->vdev);

	if (dev->hvm_irq)
		release_hvm_virq(dev->hvm_irq);
	if (dev->gvm_irq)
		release_gvm_virq(vm, dev->gvm_irq);

	free(dev);
}

static void virtio_dev_deinit(struct vdev *vdev)
{
	struct virtio_device *dev = vdev_to_virtio(vdev);

	release_virtio_dev(vdev->vm, dev);
}

void *create_virtio_device(struct vm *vm)
{
	int ret;
	struct vdev *vdev;
	struct virtio_device *virtio_dev = NULL;

	if (!vm)
		return NULL;

	virtio_dev = malloc(sizeof(struct virtio_device));
	if (!virtio_dev)
		return NULL;

	memset(virtio_dev, 0, sizeof(struct virtio_device));
	vdev = &virtio_dev->vdev;
	ret = guest_vdev_init(vm, vdev, PAGE_SIZE);
	if (ret)
		goto out;

	vdev->read = virtio_mmio_read;
	vdev->write = virtio_mmio_write;
	vdev->deinit = virtio_dev_deinit;

	virtio_dev->hvm_irq = alloc_hvm_virq();
	if (virtio_dev->hvm_irq <= 0)
		goto out;

	virtio_dev->gvm_irq = alloc_gvm_virq(vm);
	if (virtio_dev->gvm_irq <= 0)
		goto out;

	/*
	 * virtio's io memory need to mapped to host vm mem space
	 * then the backend driver can read/write the io memory
	 * by the way, this memory also need to mapped to the
	 * guest vm 's memory space
	 */
	if (create_guest_mapping(vm, vdev->gvm_paddr,
				(unsigned long)vdev->iomem,
				PAGE_SIZE, VM_IO | VM_RO))
		goto out;

	virtio_device_init(vm, virtio_dev);

	return (void *)vdev->hvm_paddr;

out:
	release_virtio_dev(vm, virtio_dev);
	return NULL;
}
