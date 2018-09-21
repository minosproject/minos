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

#include <asm/psci.h>
#include <minos/platform.h>

static struct platform platform_espressobin = {
	.name		 = "marvell_espressobin",
	.cpu_on		 = psci_cpu_on,
	.cpu_off	 = psci_cpu_off,
	.system_reboot	 = psci_system_reboot,
	.system_shutdown = psci_system_shutdown,
};

DEFINE_PLATFORM(platform_espressobin);
