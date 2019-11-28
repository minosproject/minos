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
#include <virt/vmm.h>
#include <virt/vm.h>
#include <minos/sched.h>
#include <virt/virq.h>
#include <libfdt/libfdt.h>
#include <virt/virq_chip.h>
#include <minos/of.h>
#include <asm/io.h>
#include <virt/vmbox.h>

static int hvc_fe_init(struct vm *vm, struct vmbox *vmbox,
		struct vmbox_device *vdev)
{
	struct vmbox_controller *vc;
	struct vmm_area *va;
	void *dtb = vm->setup_data;

	vc = vmbox_get_controller(vm);
	if (!vc)
		return -ENOENT;

	va = alloc_free_vmm_area(&vm->mm, vmbox->shmem_size,
			PAGE_MASK, VM_MAP_PT | VM_IO);
	if (!va)
		return -ENOMEM;
	vdev->iomem = va->start;
	vdev->iomem_size = vmbox->shmem_size;
	map_vmm_area(&vm->mm, va, (unsigned long)vmbox->shmem);

	return vmbox_register_platdev(vdev, dtb, vmbox->name);
}

static struct vmbox_hook_ops hvc_ops = {
	.vmbox_fe_init = hvc_fe_init,
};

static int vmbox_hvc_init(void)
{
	return register_vmbox_hook("hvc", &hvc_ops);
}
subsys_initcall(vmbox_hvc_init);
