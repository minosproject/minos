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

#include <mvm.h>
#include <virtio.h>
#include <virtio_mmio.h>
#include <io.h>

void *hv_create_virtio_device(struct vm *vm)
{
	int ret;
	void *iomem;

	ret = ioctl(vm->vm_fd, IOCTL_CREATE_VIRTIO_DEVICE, &iomem);
	if (ret) {
		printf("create virtio device failed %d\n", ret);
		return NULL;
	}

	return iomem;
}

int virtio_device_init(struct vdev *vdev, void *iomem, int device_id)
{
	void *base;

	vdev->iomem_physic = iomem;
	base = vdev_map_iomem(iomem, 4096);
	if (base == (void *)-1)
		return -ENOMEM;

	vdev->iomem = base;
	vdev->gvm_irq = ioread32(base + VIRTIO_MMIO_GVM_IRQ);
	vdev->hvm_irq = ioread32(base + VIRTIO_MMIO_HVM_IRQ);

	/* TO BE FIX need to covert to 64bit address */
	vdev->guest_iomem = (unsigned long)ioread32(base +
			VIRTIO_MMIO_GVM_ADDR);

	printv("vdev : %d %d 0x%lx 0x%lx\n", vdev->gvm_irq,
			vdev->hvm_irq, (unsigned long)vdev->iomem,
			vdev->guest_iomem);

	iowrite32(base + VIRTIO_MMIO_MAGIC_VALUE, VIRTIO_MMIO_MAGIG);
	iowrite32(base + VIRTIO_MMIO_VERSION, VIRTIO_VERSION);
	iowrite32(base + VIRTIO_MMIO_VENDOR_ID, VIRTIO_VENDER_ID);
	iowrite32(base + VIRTIO_MMIO_DEVICE_ID, device_id);

	return 0;
}
