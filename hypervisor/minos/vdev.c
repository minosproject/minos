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
#include <minos/sched.h>

void vdev_set_name(struct vdev *vdev, char *name)
{
	int len;

	if (!vdev || !name)
		return;

	len = strlen(name);
	if (len > VDEV_NAME_SIZE)
		len = VDEV_NAME_SIZE;

	strncpy(vdev->name, name, len);
}

void vdev_release(struct vdev *vdev)
{
	if (vdev->iomem)
		free(vdev->iomem);

	if (vdev->hvm_paddr)
		destroy_hvm_iomem_map(vdev->hvm_paddr, vdev->mem_size);

	if (vdev->list.next != NULL)
		list_del(&vdev->list);
}

static void vdev_deinit(struct vdev *vdev)
{
	pr_warn("using default vdev deinit routine\n");
	vdev_release(vdev);
	free(vdev);
}

int iomem_vdev_init(struct vm *vm, struct vdev *vdev, uint32_t size)
{
	struct mm_struct *mm = &vm->mm;

	size = PAGE_BALIGN(size);

	if (mm->gvm_iomem_size < size)
		return -ENOMEM;

	memset(vdev, 0, sizeof(struct vdev));

	if (size > 0) {
		vdev->iomem = get_io_pages(PAGE_NR(size));
		if (!vdev->iomem) {
			free(vdev);
			return -ENOMEM;
		}

		vdev->hvm_paddr = create_hvm_iomem_map((unsigned long)
				vdev->iomem, size);
		if (!vdev->hvm_paddr)
			return -ENOMEM;

		memset(vdev->iomem, 0, size);
	}

	vdev->gvm_paddr = mm->gvm_iomem_base;
	vdev->mem_size = size;
	mm->gvm_iomem_base += size;
	mm->gvm_iomem_size -= size;
	vdev->vm = vm;
	vdev->deinit = vdev_deinit;
	vdev->list.next = NULL;
	vdev->host = 0;
	list_add_tail(&vm->vdev_list, &vdev->list);

	return 0;
}

struct vdev *create_iomem_vdev(struct vm *vm, uint32_t size)
{
	struct vdev *vdev;

	vdev = malloc(sizeof(struct vdev));
	if (!vdev)
		return NULL;

	if (iomem_vdev_init(vm, vdev, size)) {
		free(vdev);
		return NULL;
	}

	return vdev;
}

int host_vdev_init(struct vm *vm, struct vdev *vdev,
		unsigned long base, uint32_t size)
{
	if (!vdev)
		return -EINVAL;

	pr_info("vdev init with addr@0x%p size@0x%x\n", base, size);

	memset(vdev, 0, sizeof(struct vdev));
	vdev->gvm_paddr = base;
	vdev->mem_size = size;
	vdev->vm = vm;
	vdev->host = 1;
	vdev->list.next = NULL;
	vdev->deinit = vdev_deinit;
	list_add_tail(&vm->vdev_list, &vdev->list);

	return 0;
}

struct vdev *create_host_vdev(struct vm *vm, unsigned long base, uint32_t size)
{
	struct vdev *vdev;

	size = PAGE_BALIGN(size);

	vdev = malloc(sizeof(struct vdev));
	if (!vdev)
		return NULL;

	host_vdev_init(vm, vdev, base, size);

	return vdev;
}

unsigned long create_guest_vdev(struct vm *vm, uint32_t size)
{
	struct mm_struct *mm = &vm->mm;
	unsigned long vdev_base;

	/*
	 * for the vdev which totally handled in the
	 * host vm, just update the gvm_iomem_base and the
	 * gvm_iomem_size, hypervisor will trap the event
	 * to hvm directly
	 */
	size = PAGE_BALIGN(size);
	if (mm->gvm_iomem_size < size)
		return 0;

	vdev_base = mm->gvm_iomem_base;
	mm->gvm_iomem_base += size;
	mm->gvm_iomem_size -= size;

	return vdev_base;
}

int vdev_mmio_emulation(gp_regs *regs, int write,
		unsigned long address, unsigned long *value)
{
	struct vm *vm = current_vm;
	struct vdev *vdev;

	list_for_each_entry(vdev, &vm->vdev_list, list) {
		if ((address >= vdev->gvm_paddr) &&
			(address < vdev->gvm_paddr + vdev->mem_size)) {
			if (write)
				return vdev->write(vdev, regs, address, value);
			else
				return vdev->read(vdev, regs, address, value);
		}
	}

	/*
	 * trap the mmio rw event to hvm if there is no vdev
	 * in host can handle it
	 */
	return trap_vcpu(VMTRAP_TYPE_MMIO, write, address, value);
}
