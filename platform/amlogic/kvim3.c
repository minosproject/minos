/*
 * Copyright (C) 2019 Min Le (lemin9538@gmail.com)
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
#include <asm/arch.h>
#include <asm/io.h>
#include <minos/mmu.h>
#include <libfdt/libfdt.h>
#include <minos/of.h>
#include <minos/platform.h>
#include <asm/power.h>

#ifdef CONFIG_VIRT
#include <virt/vm.h>

static int kvim3_setup_hvm(struct vm *vm, void *data)
{
	return 0;
}

#endif

static struct platform platform_kvim3 = {
	.name		 = "khadas,vim3",
	.cpu_on		 = psci_cpu_on,
	.cpu_off	 = psci_cpu_off,
#ifdef CONFIG_VIRT
	.setup_hvm	 = kvim3_setup_hvm,
#endif
	.system_reboot	 = psci_system_reboot,
	.system_shutdown = psci_system_shutdown,
};
DEFINE_PLATFORM(platform_kvim3);
