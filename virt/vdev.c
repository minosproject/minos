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
#include <minos/sched.h>
#include <virt/vdev.h>
#include <virt/virq.h>
#include <virt/vmcs.h>

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

}

static void vdev_deinit(struct vdev *vdev)
{
	pr_warn("using default vdev deinit routine\n");
	vdev_release(vdev);
	free(vdev);
}

int host_vdev_init(struct vm *vm, struct vdev *vdev,
		unsigned long base, uint32_t size)
{
	if (!vdev)
		return -EINVAL;

	pr_debug("vdev init with addr@0x%p size@0x%x\n", base, size);

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

	vdev = malloc(sizeof(*vdev));
	if (!vdev)
		return NULL;

	host_vdev_init(vm, vdev, base, size);

	return vdev;
}

int vdev_mmio_emulation(gp_regs *regs, int write,
		unsigned long address, unsigned long *value)
{
	struct vm *vm = get_current_vm();
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
