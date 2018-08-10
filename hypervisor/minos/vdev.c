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
#include <minos/virq.h>

void vdev_release(struct vdev *vdev)
{
	if (vdev->iomem)
		free(vdev->iomem);

	if (vdev->hvm_paddr)
		destroy_hvm_iomem_map(vdev->hvm_paddr, vdev->mem_size);

	list_del(&vdev->list);
}

static void vdev_deinit(struct vdev *vdev)
{
	vdev_release(vdev);
	free(vdev);
}

int vdev_init(struct vm *vm, struct vdev *vdev, uint32_t size)
{
	struct mm_struct *mm = &vm->mm;

	size = PAGE_BALIGN(size);

	if (mm->gvm_iomem_size < size)
		return -ENOMEM;

	memset(vdev, 0, sizeof(struct vdev));

	if (size > 0) {
		vdev->iomem = get_free_pages(PAGE_NR(size));
		if (!vdev->iomem) {
			free(vdev);
			return -ENOMEM;
		}

		vdev->hvm_paddr = create_hvm_iomem_map((unsigned long)
				vdev->iomem, PAGE_SIZE);
		if (!vdev->hvm_paddr)
			return -ENOMEM;

		memset(vdev->iomem, 0, size);
	}

	vdev->gvm_paddr = mm->gvm_iomem_base;
	vdev->mem_size = size;
	mm->gvm_iomem_base -= size;
	mm->gvm_iomem_size -= size;
	vdev->vm = vm;
	vdev->deinit = vdev_deinit;
	list_add_tail(&vm->vdev_list, &vdev->list);

	return 0;
}

struct vdev *create_guest_vdev(struct vm *vm, uint32_t size)
{
	struct vdev *vdev;

	vdev = malloc(sizeof(struct vdev));
	if (!vdev)
		return NULL;

	if (vdev_init(vm, vdev, size)) {
		free(vdev);
		return NULL;
	}

	return vdev;
}

struct vdev *create_host_vdev(struct vm *vm, unsigned long base, uint32_t size)
{
	struct vdev *vdev;

	size = PAGE_BALIGN(size);

	vdev = malloc(sizeof(struct vdev));
	if (!vdev)
		return NULL;

	memset(vdev, 0, sizeof(struct vdev));
	vdev->gvm_paddr = base;
	vdev->mem_size = size;
	vdev->vm = vm;
	vdev->deinit = vdev_deinit;
	list_add_tail(&vm->vdev_list, &vdev->list);

	return 0;
}
