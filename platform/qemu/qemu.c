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
#include <asm/power.h>
#include <asm/io.h>
#include <minos/platform.h>
#include <libfdt/libfdt.h>
#ifdef CONFIG_VIRT
#include <virt/vm.h>
#include <virt/vmm.h>
#endif

#ifdef CONFIG_VIRT
static int qemu_setup_hvm(struct vm *vm, void *data)
{
	int ret;

	/*
	 * workaroud, create the PCIE memory region for VM
	 * since currently minos do not support parsing the
	 * PCI range, TBD.
	 * pci_bus 0000:00: root bus resource [bus 00-ff]
	 * pci_bus 0000:00: root bus resource [io  0x0000-0xffff]
	 * pci_bus 0000:00: root bus resource [mem 0x10000000-0x3efeffff]
	 * pci_bus 0000:00: root bus resource [mem 0x8000000000-0xffffffffff]
	 */
	split_vmm_area(&vm->mm, 0x0, 0x10000, VM_GUEST_IO | VM_RW);
	split_vmm_area(&vm->mm, 0x10000000, 0x2EFF0000,
			VM_GUEST_IO | VM_RW | VM_HUGE);
	split_vmm_area(&vm->mm, 0x8000000000, 0x8000000000,
			VM_GUEST_IO | VM_RW | __VM_HUGE_1G);

	ret = create_guest_mapping(&vm->mm, 0x0, 0x0, 0x10000, VM_GUEST_IO | VM_RW);
	ret += create_guest_mapping(&vm->mm, 0x10000000, 0x10000000, 0x2EFF0000,
			VM_GUEST_IO | VM_RW | VM_HUGE);
	ret += create_guest_mapping(&vm->mm, 0x8000000000, 0x8000000000,
			0x8000000000, VM_GUEST_IO | VM_RW | __VM_HUGE_1G);
	if (ret)
		pr_err("map PCIE memory region for guest failed\n");

	return ret;
}
#endif

static struct platform platform_qemu = {
	.name 		 = "linux,qemu-arm64",
#ifdef CONFIG_VIRT
	.cpu_on		 = psci_cpu_on,
	.cpu_off	 = psci_cpu_off,
	.system_reboot	 = psci_system_reboot,
	.system_shutdown = psci_system_shutdown,
	.setup_hvm	 = qemu_setup_hvm,
#else
	.cpu_on		 = psci_cpu_on_hvc,
	.cpu_off	 = psci_cpu_off_hvc,
	.system_reboot	 = psci_system_reboot_hvc,
	.system_shutdown = psci_system_shutdown_hvc,
#endif
};

DEFINE_PLATFORM(platform_qemu);
