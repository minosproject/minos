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
#include <asm/io.h>
#include <minos/sched.h>
#include <minos/vdev.h>
#include <minos/virq.h>
#include <common/virtio_mmio.h>
#include <minos/resource.h>

#define vdev_to_virtio(vd) \
	container_of(vd, struct virtio_device, vdev)

static int virtio_mmio_read(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *read_value)
{
	return 0;
}

static int virtio_mmio_write(struct vdev *vdev, gp_regs *regs,
		unsigned long address, unsigned long *write_value)
{
	uint32_t tmp;
	uint32_t value = *(uint32_t *)write_value;
	void *iomem = vdev->iomem;
	unsigned long offset = address - vdev->gvm_paddr;

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
		iowrite32(tmp, iomem + VIRTIO_MMIO_HOST_FEATURES);
		break;
	case VIRTIO_MMIO_GUEST_FEATURES_SEL:
		iowrite32(value, iomem + VIRTIO_MMIO_GUEST_FEATURES_SEL);
		break;
	case VIRTIO_MMIO_GUEST_FEATURES:
		tmp = ioread32(iomem + VIRTIO_MMIO_GUEST_FEATURES_SEL);
		tmp = tmp * sizeof(uint32_t) + VIRTIO_MMIO_DRIVER_FEATURE0;
		iowrite32(value, iomem + tmp);
		break;
	case VIRTIO_MMIO_GUEST_PAGE_SIZE:
		/* version 1 virtio device */
		iowrite32(value, iomem + VIRTIO_MMIO_GUEST_PAGE_SIZE);
		break;
	case VIRTIO_MMIO_QUEUE_SEL:
		iowrite32(value, iomem + VIRTIO_MMIO_QUEUE_SEL);

		/* clear the queue information in the memory */
		iowrite32(0, iomem + VIRTIO_MMIO_QUEUE_READY);
		iowrite32(0, iomem + VIRTIO_MMIO_QUEUE_NUM);
		iowrite32(0, iomem + VIRTIO_MMIO_QUEUE_DESC_LOW);
		iowrite32(0, iomem + VIRTIO_MMIO_QUEUE_DESC_HIGH);
		iowrite32(0, iomem + VIRTIO_MMIO_QUEUE_AVAIL_LOW);
		iowrite32(0, iomem + VIRTIO_MMIO_QUEUE_AVAIL_HIGH);
		iowrite32(0, iomem + VIRTIO_MMIO_QUEUE_USED_LOW);
		iowrite32(0, iomem + VIRTIO_MMIO_QUEUE_USED_HIGH);
		break;
	case VIRTIO_MMIO_QUEUE_NUM:
		tmp = ioread32(iomem + VIRTIO_MMIO_QUEUE_NUM_MAX);
		if (value > tmp)
			pr_warn("invalid queue sel %d\n", value);
		iowrite32(value, iomem + VIRTIO_MMIO_QUEUE_NUM);
		break;
	case VIRTIO_MMIO_QUEUE_ALIGN:
		iowrite32(value, iomem + VIRTIO_MMIO_QUEUE_ALIGN);
		break;
	case VIRTIO_MMIO_QUEUE_PFN:
		/* this is for version 1 virtio device */
		iowrite32(value, iomem + VIRTIO_MMIO_QUEUE_PFN);
		break;
	case VIRTIO_MMIO_QUEUE_NOTIFY:
		/*
		 * indicate a queue is ready, need send a
		 * event to hvm ?
		 */
		trap_mmio_write_nonblock(address, write_value);
		break;
	case VIRTIO_MMIO_STATUS:
		tmp = ioread32(iomem + VIRTIO_MMIO_STATUS);
		value = value - tmp;
		*write_value = value;
		iowrite32(value, iomem + VIRTIO_MMIO_STATUS);
		trap_mmio_write(address, write_value);
		break;
	case VIRTIO_MMIO_QUEUE_DESC_LOW:
		iowrite32(value, iomem + VIRTIO_MMIO_QUEUE_DESC_LOW);
		break;
	case VIRTIO_MMIO_QUEUE_AVAIL_LOW:
		iowrite32(value, iomem + VIRTIO_MMIO_QUEUE_AVAIL_LOW);
		break;
	case VIRTIO_MMIO_QUEUE_USED_LOW:
		iowrite32(value, iomem + VIRTIO_MMIO_QUEUE_USED_LOW);
		break;
	case VIRTIO_MMIO_QUEUE_DESC_HIGH:
		iowrite32(value, iomem + VIRTIO_MMIO_QUEUE_DESC_HIGH);
		break;
	case VIRTIO_MMIO_QUEUE_USED_HIGH:
		iowrite32(value, iomem + VIRTIO_MMIO_QUEUE_USED_HIGH);
		break;
	case VIRTIO_MMIO_QUEUE_AVAIL_HIGH:
		iowrite32(value, iomem + VIRTIO_MMIO_QUEUE_AVAIL_HIGH);
		break;
	case VIRTIO_MMIO_QUEUE_READY:
		value = ioread32(iomem + VIRTIO_MMIO_QUEUE_SEL);
		*write_value = value;
		trap_mmio_write(address, write_value);
		break;
	case VIRTIO_MMIO_INTERRUPT_ACK:
		iowrite32(0, iomem + VIRTIO_MMIO_INTERRUPT_ACK);
		iowrite32(0, iomem + VIRTIO_MMIO_INTERRUPT_STATUS);
		break;
	default:
		trap_mmio_write(address, write_value);
		break;
	}

	return 0;
}

void release_virtio_dev(struct vm *vm, struct virtio_device *dev)
{
	if (!dev)
		return;

	vdev_release(&dev->vdev);
	free(dev);
}

static void virtio_dev_deinit(struct vdev *vdev)
{
	struct virtio_device *dev = vdev_to_virtio(vdev);

	release_virtio_dev(vdev->vm, dev);
}

static void virtio_dev_reset(struct vdev *vdev)
{
	pr_info("virtio device reset\n");
}

static void *virtio_create_device(struct vm *vm, struct device_node *node)
{
	int ret;
	uint32_t irq;
	unsigned long offset = 0;
	struct vdev *vdev;
	struct virtio_device *virtio_dev = NULL;
	struct mm_struct *mm = &vm->mm;
	uint64_t base, size;
	unsigned long flags;

	pr_info("create virtio-mmio device for vm-%d\n", vm->vmid);

	if (!mm->virtio_mmio_iomem)
		return NULL;

	ret = translate_device_address(node, &base, &size);
	if (ret || (size == 0))
		return NULL;

	ret = get_device_irq_index(vm, node, &irq, &flags, 0);
	if (ret)
		return NULL;

	if (base >= (mm->virtio_mmio_gbase + mm->virtio_mmio_size)) {
		pr_err("invalid virtio mmio range 0x%x\n", base);
		return NULL;
	}

	virtio_dev = malloc(sizeof(struct virtio_device));
	if (!virtio_dev)
		return NULL;

	memset(virtio_dev, 0, sizeof(struct virtio_device));
	vdev = &virtio_dev->vdev;
	ret = host_vdev_init(vm, vdev, base, VIRTIO_DEVICE_IOMEM_SIZE);
	if (ret)
		goto out;

	request_virq(vm, irq, 0);

	/* set up the iomem base of the vdev */
	offset = base - mm->virtio_mmio_gbase;
	vdev->iomem = mm->virtio_mmio_iomem + offset;

	vdev->read = virtio_mmio_read;
	vdev->write = virtio_mmio_write;
	vdev->deinit = virtio_dev_deinit;
	vdev->reset = virtio_dev_reset;

	return vdev;

out:
	release_virtio_dev(vm, virtio_dev);
	return NULL;
}
VDEV_DECLARE(virtio_mmio, virtio_match_table, virtio_create_device);

int virtio_mmio_init(struct vm *vm, size_t size,
		unsigned long *gbase, unsigned long *hbase)
{
	void *iomem = NULL;
	unsigned long __gbase = 0;
	struct mm_struct *mm = &vm->mm;

	if (mm->virtio_mmio_iomem) {
		pr_err("virtio mmio has been inited\n");
		return -EINVAL;
	}

	if (size == 0) {
		pr_err("invaild virtio mmio size\n");
		return -EINVAL;
	}

	*gbase = 0;
	*hbase = 0;
	size = PAGE_BALIGN(size);

	__gbase = create_guest_vdev(vm, size);
	if (__gbase == 0)
		return -ENOMEM;

	iomem = get_io_pages(PAGE_NR(size));
	if (!iomem)
		return -ENOMEM;

	memset(iomem, 0, size);

	/*
	 * virtio's io memory need to mapped to host vm mem space
	 * then the backend driver can read/write the io memory
	 * by the way, this memory also need to mapped to the
	 * guest vm 's memory space
	 */
	*hbase = create_hvm_iomem_map((unsigned long)iomem, size);
	if (*hbase == 0) {
		free_pages(iomem);
		return -ENOMEM;
	}

	if (create_guest_mapping(vm, __gbase, (unsigned long)iomem,
				size, VM_IO | VM_RO)) {
		free_pages(iomem);
		return -EFAULT;
	}

	/* update the virtio information of the vm */
	mm->virtio_mmio_gbase = __gbase;
	mm->virtio_mmio_iomem = iomem;
	mm->virtio_mmio_size = size;

	*gbase = __gbase;

	return 0;
}

int virtio_mmio_deinit(struct vm *vm)
{
	struct mm_struct *mm = &vm->mm;

	if (mm->virtio_mmio_iomem)
		free_pages(mm->virtio_mmio_iomem);

	mm->virtio_mmio_gbase = 0;
	mm->virtio_mmio_iomem = NULL;
	mm->virtio_mmio_size = 0;

	return 0;
}
