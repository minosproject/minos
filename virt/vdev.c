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
#include <virt/vmm.h>
#include <virt/vm.h>

static void vdev_set_name(struct vdev *vdev, const char *name)
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
	struct vmm_area *vma = vdev->gvm_area;
	struct vmm_area *next;

	if (vdev->list.next != NULL)
		list_del(&vdev->list);

	/*
	 * release the vmm_areas if has. just delete it, the VMM
	 * will release all the vmm_area of VM.
	 */
	while (vma) {
		next = vma->next;
		vma->next = NULL;
		vma = next;
	}
}

static void vdev_deinit(struct vdev *vdev)
{
	pr_warn("using default vdev deinit routine\n");
	vdev_release(vdev);
	free(vdev);
}

void host_vdev_init(struct vm *vm, struct vdev *vdev, const char *name)
{
	if (!vm || !vdev) {
		pr_err("%s: no such VM or VDEV\n");
		return;
	}

	memset(vdev, 0, sizeof(struct vdev));
	vdev->vm = vm;
	vdev->host = 1;
	vdev->list.next = NULL;
	vdev->deinit = vdev_deinit;
	vdev->list.next = NULL;
	vdev->list.pre = NULL;
	vdev_set_name(vdev, name);
}

static void inline vdev_add_vmm_area(struct vdev *vdev, struct vmm_area *va)
{
	struct vmm_area *head = vdev->gvm_area;
	struct vmm_area *prev = NULL;

	/*
	 * add to the list tail.
	 */
	while (head) {
		prev = head;
		head = head->next;
	}

	va->next = NULL;
	if (prev == NULL)
		vdev->gvm_area = va;
	else
		prev->next = va;
}

int vdev_add_iomem_range(struct vdev *vdev, unsigned long base, size_t size)
{
	struct vmm_area *va;

	if (!vdev || !vdev->vm)
		return -ENOENT;

	/*
	 * vdev memory usually will not mapped to the real
	 * physical space, here set the flags to 0.
	 */
	va = split_vmm_area(&vdev->vm->mm, base, size, VM_GUEST_VDEV);
	if (!va) {
		pr_err("vdev: request vmm area failed 0x%lx 0x%lx\n",
				base, base + size);
		return -ENOMEM;
	}

	vdev_add_vmm_area(vdev, va);

	return 0;
}

void vdev_add(struct vdev *vdev)
{
	if (!vdev->vm)
		pr_err("%s vdev has not been init\n");
	else
		list_add_tail(&vdev->vm->vdev_list, &vdev->list);
}

struct vmm_area *vdev_alloc_iomem_range(struct vdev *vdev, size_t size, int flags)
{
	struct vmm_area *va;

	va = alloc_free_vmm_area(&vdev->vm->mm, size, PAGE_MASK, flags);
	if (!va)
		return NULL;

	vdev_add_vmm_area(vdev, va);

	return va;
}

struct vmm_area *vdev_get_vmm_area(struct vdev *vdev, int idx)
{
	struct vmm_area *va = vdev->gvm_area;

	while (idx || !va) {
		va = va->next;
		idx--;
	}

	return va;
}

struct vdev *create_host_vdev(struct vm *vm, const char *name)
{
	struct vdev *vdev;

	vdev = malloc(sizeof(*vdev));
	if (!vdev)
		return NULL;

	host_vdev_init(vm, vdev, name);

	return vdev;
}

static inline int handle_mmio_write(struct vdev *vdev, gp_regs *regs,
		int idx, unsigned long offset, unsigned long *value)
{
	if (vdev->write)
		return vdev->write(vdev, regs, idx, offset, value);
	else
		return 0;
}

static inline int handle_mmio_read(struct vdev *vdev, gp_regs *regs,
		int idx, unsigned long offset, unsigned long *value)
{
	if (vdev->read)
		return vdev->read(vdev, regs, idx, offset, value);
	else
		return 0;
}

static inline int handle_mmio(struct vdev *vdev, gp_regs *regs, int write,
		int idx, unsigned long offset, unsigned long *value)
{
	if (write)
		return handle_mmio_write(vdev, regs, idx, offset, value);
	else
		return handle_mmio_read(vdev, regs, idx, offset, value);
}

int vdev_mmio_emulation(gp_regs *regs, int write,
		unsigned long address, unsigned long *value)
{
	struct vm *vm = get_current_vm();
	struct vdev *vdev;
	struct vmm_area *va;
	int idx, ret = 0;

	list_for_each_entry(vdev, &vm->vdev_list, list) {
		idx = 0;
		va = vdev->gvm_area;
		while (va) {
			if ((address >= va->start) && (address <= va->end)) {
				ret = handle_mmio(vdev, regs, write,
						idx, address - va->start, value);
				if (ret)
					pr_warn("vm%d %s mmio 0x%lx in %s failed\n", vm->vmid,
							write ? "write" : "read", address, vdev->name);
				return 0;
			}
			idx++;
			va = va->next;
		}
	}

	/*
	 * trap the mmio rw event to hvm if there is no vdev
	 * in host can handle it
	 */
	if (vm_is_native(vm))
		return -EFAULT;

	ret = trap_vcpu(VMTRAP_TYPE_MMIO, write, address, value);
	if (ret) {
		pr_warn("vm%d %s mmio 0x%lx failed %d\n", vm->vmid,
				write ? "write" : "read", address, ret);
		ret = (ret == -EACCES) ? -EACCES : 0;
	}

	return ret;
}
