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
#include <minos/percpu.h>
#include <minos/sched.h>

extern unsigned char __percpu_start;
extern unsigned char __percpu_end;
extern unsigned char __percpu_section_size;

unsigned long percpu_offset[CONFIG_NR_CPUS];

void percpus_init(void)
{
	int i;
	size_t size;

	size = (&__percpu_end) - (&__percpu_start);
	memset((char *)&__percpu_start, 0, size);

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		percpu_offset[i] = (phy_addr_t)(&__percpu_start) +
			(size_t)(&__percpu_section_size) * i;
	}
}
