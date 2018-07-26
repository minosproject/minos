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
#include <minos/io.h>

static void *pc_base = (void *)0x1c10000;

void power_on_cpu_core(uint8_t aff0,
		uint8_t aff1, uint8_t aff2)
{
	uint32_t mpid = 0;

	mpid = (aff0 << 0) | (aff1 << 8) | (aff2 << 16);
	iowrite32(pc_base + PPONR, mpid);
}

void power_off_cpu_core(uint8_t aff0,
		uint8_t aff1, uint8_t aff2)
{
	uint32_t mpid = 0;

	mpid = (aff0 << 0) | (aff1 << 8) | (aff2 << 16);
	iowrite32(pc_base + PPOFFR, mpid);
}
