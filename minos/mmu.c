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
#include <minos/mmu.h>
#include <minos/mm.h>

static struct mmu_chip *mmu_chip;

int mmu_map_guest_memory(unsigned long tbase, unsigned long vir_base,
		unsigned long phy_base, size_t size, int type)
{
	if (!mmu_chip->map_guest_memory)
		return -EINVAL;

	return mmu_chip->map_guest_memory(tbase, phy_base,
			vir_base, size, type);
}

unsigned long mmu_alloc_guest_pt(void)
{
	if (!mmu_chip->alloc_guest_pt)
		return -EINVAL;

	return mmu_chip->alloc_guest_pt();
}

int mmu_map_host_memory(unsigned long vir,
		unsigned long phy, size_t size, int type)
{
	if (!mmu_chip->map_host_memory)
		return -EINVAL;

	return mmu_chip->map_host_memory(vir, phy, size, type);
}

int io_remap(unsigned long vir, unsigned long phy, size_t size)
{
	if (!mmu_chip->map_host_memory)
		return -EINVAL;

	return mmu_chip->map_host_memory(vir, phy, size, MEM_TYPE_IO);
}

static int check_mmuchip(struct module_id *vmodule)
{
	return (!(strcmp(vmodule->name, CONFIG_MMU_CHIP_NAME)));
}

int mmu_init(void)
{
	extern unsigned char __mmuchip_start;
	extern unsigned char __mmuchip_end;
	unsigned long s, e;

	s = (unsigned long)&__mmuchip_start;
	e = (unsigned long)&__mmuchip_end;

	mmu_chip = (struct mmu_chip *)get_module_pdata(s, e, check_mmuchip);
	if (!mmu_chip)
		panic("can not find the mmuchip for system\n");

	return 0;
}
