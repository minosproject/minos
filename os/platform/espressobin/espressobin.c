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

#include <asm/cpu.h>
#include <minos/mm.h>
#include <virt/vmm.h>
#include <minos/platform.h>

#ifdef CONFIG_VIRT
static int espressobin_setup_vm0(struct vm *vm, void *dtb)
{
	/* create the pcie region for the vm0 */
	create_guest_mapping(&vm->mm, 0xe8000000, 0xe8000000, 0x1000000, VM_IO);
	create_guest_mapping(&vm->mm, 0xe9000000, 0xe9000000, 0x10000, VM_IO);

	return 0;
}
#endif

static struct platform platform_espressobin = {
	.name		 = "marvell,armada-3720-community",
	.cpu_on		 = psci_cpu_on,
	.cpu_off	 = psci_cpu_off,
	.system_reboot	 = psci_system_reboot,
	.system_shutdown = psci_system_shutdown,
#ifdef CONFIG_VIRT
	.setup_hvm	 = espressobin_setup_vm0,
#endif
};

DEFINE_PLATFORM(platform_espressobin);
