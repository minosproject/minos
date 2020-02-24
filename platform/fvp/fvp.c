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
#include <asm/cpu.h>
#include <asm/vtimer.h>
#include <asm/io.h>
#include <minos/platform.h>
#include <libfdt/libfdt.h>
#include <minos/mmu.h>

#ifdef CONFIG_VIRT
#include <virt/vm.h>
#endif

static int fvp_time_init(void)
{
	io_remap(0x2a430000, 0x2a430000, 64 * 1024);

	/* enable the counter */
	iowrite32(1, (void *)0x2a430000 + REG_CNTCR);
	return 0;
}

#ifdef CONFIG_VIRT
static int inline fvp_setup_hvm_of(struct vm *vm, void *data)
{
	int node, len;
	const void *val;

	/* disable the armv7-timer-mem for fvp*/
	node = fdt_path_offset(data, "/timer@2a810000");
	if (node < 0)
		return 0;

	val = fdt_getprop(data, node, "compatible", &len);
	if (!val || len <= 0)
		return 0;

	if (!strcmp((char *)val, "arm,armv7-timer-mem")) {
		pr_info("delete the armv7 mem timer\n");
		fdt_del_node(data, node);
	}

	return 0;
}

static int fvp_setup_hvm(struct vm *vm, void *data)
{
	if (vm->flags & VM_FLAGS_SETUP_OF)
		return fvp_setup_hvm_of(vm, data);

	return 0;
}
#endif

static struct platform platform_fvp = {
	.name 		 = "arm,fvp-base",
	.time_init 	 = fvp_time_init,
	.cpu_on		 = psci_cpu_on,
	.cpu_off	 = psci_cpu_off,
#ifdef CONFIG_VIRT
	.setup_hvm	 = fvp_setup_hvm,
#endif
	.system_reboot	 = psci_system_reboot,
	.system_shutdown = psci_system_shutdown,
};

DEFINE_PLATFORM(platform_fvp);
