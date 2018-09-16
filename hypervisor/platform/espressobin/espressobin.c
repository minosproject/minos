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

extern int mvebu_serial_probe(void *addr);

int platform_serial_init(void)
{
	return mvebu_serial_probe((void *)0xd0012000);
}

int platform_cpu_on(int cpu, unsigned long entry)
{
	return psci_cpu_on(cpu, entry);
}

int platform_time_init(void)
{
	return 0;
}
