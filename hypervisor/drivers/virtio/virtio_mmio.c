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
#include <minos/virtio_mmio.h>
#include <minos/mmio.h>
#include <minos/io.h>
#include <minos/sched.h>
#include <minos/vdev.h>
#include <minos/virq.h>

#define vdev_to_virtio(vd) container_of(vd, struct virtio_device, vdev)

static int virtio_mmio_read(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *read_value)
{
	return 0;
}

static int virtio_mmio_write(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *write_value)
{
	void *iomem = vdev->iomem;
	unsigned long offset = address - vdev->gvm_paddr;

	switch (offset) {
	case VIRTIO_MMIO_HOST_FEATURES:
		break;
	case VIRTIO_MMIO_HOST_FEATURES_SEL:
		break;
	case VIRTIO_MMIO_GUEST_PAGE_SIZE:
		break;
	case VIRTIO_MMIO_QUEUE_SEL:
		break;
	case VIRTIO_MMIO_QUEUE_NUM:
		break;
	case VIRTIO_MMIO_QUEUE_ALIGN:
		break;
	case VIRTIO_MMIO_QUEUE_PFN:
		break;
	case VIRTIO_MMIO_QUEUE_NOTIFY:
		break;
	case VIRTIO_MMIO_INTERRUPT_ACK:
		break;
	case VIRTIO_MMIO_STATUS:
		break;
	case VIRTIO_MMIO_QUEUE_DESC_LOW:
		break;
	case VIRTIO_MMIO_QUEUE_AVAIL_LOW:
		break;
	case VIRTIO_MMIO_QUEUE_USED_LOW:
		break;
	default:
		break;
	}

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
	ret = vdev_init(vm, vdev, PAGE_SIZE);
	if (ret)
		goto out;

	vdev->read = virtio_mmio_read;
	vdev->write = virtio_mmio_write;
	vdev->deinit = virtio_dev_deinit;

	virtio_dev->hvm_irq = alloc_hvm_virq();
	if (virtio_dev->hvm_irq < 0)
		goto out;

	virtio_dev->gvm_irq = alloc_gvm_virq(vm);
	if (virtio_dev->gvm_irq < 0)
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
