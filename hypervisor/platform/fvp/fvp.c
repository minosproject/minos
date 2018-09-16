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
#include <minos/io.h>
#include <minos/vmm.h>

void *refclk_cnt_base = (void *)0x2a430000;

extern int pl011_init(void *addr);

int platform_serial_init(void)
{
	return pl011_init((void *)0x1c090000);
}

int platform_cpu_on(int cpu, unsigned long entry)
{
	return psci_cpu_on(cpu, entry);
}

int platform_time_init(void)
{
	io_remap(0x2a430000, 0x2a430000, 64 * 1024);

	/* enable the counter */
	iowrite32(refclk_cnt_base + CNTCR, 1);
	return 0;
}
