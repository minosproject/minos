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

static struct platform platform_qemu = {
	.name 		 = "linux,qemu-arm64",
#ifdef CONFIG_VIRT
	.cpu_on		 = psci_cpu_on,
	.cpu_off	 = psci_cpu_off,
	.system_reboot	 = psci_system_reboot,
	.system_shutdown = psci_system_shutdown,
#else
	.cpu_on		 = psci_cpu_on_hvc,
	.cpu_off	 = psci_cpu_off_hvc,
	.system_reboot	 = psci_system_reboot_hvc,
	.system_shutdown = psci_system_shutdown_hvc,
#endif
};

DEFINE_PLATFORM(platform_qemu);
