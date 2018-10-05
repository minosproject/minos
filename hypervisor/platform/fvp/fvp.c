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
#include <asm/psci.h>
#include <asm/vtimer.h>
#include <asm/io.h>
#include <minos/vmm.h>

static int fvp_time_init(void)
{
	io_remap(0x2a430000, 0x2a430000, 64 * 1024);

	/* enable the counter */
	iowrite32(1, (void *)0x2a430000 + CNTCR);
	return 0;
}

static struct platform platform_fvp = {
	.name 		 = "fvp",
	.time_init 	 = fvp_time_init,
	.cpu_on		 = psci_cpu_on,
	.cpu_off	 = psci_cpu_off,
	.system_reboot	 = psci_system_reboot,
	.system_shutdown = psci_system_shutdown,
};

DEFINE_PLATFORM(platform_fvp);
