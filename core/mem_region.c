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
#include <minos/mm.h>
#include <minos/memory.h>

#define MAX_MEMORY_REGION 16

static struct memory_region memory_regions[MAX_MEMORY_REGION];
static int current_region_id;
LIST_HEAD(mem_list);

struct memory_region *alloc_memory_region(void)
{
	ASSERT(current_region_id < MAX_MEMORY_REGION);
	return &memory_regions[current_region_id++];
}

int add_memory_region(uint64_t base, uint64_t size, int type, int vmid)
{
	phy_addr_t r_base, r_end;
	size_t r_size;
	struct memory_region *region;

	if ((size == 0) || (type >= MEMORY_REGION_TYPE_MAX))
		return -EINVAL;

	/*
	 * need to check whether this region is confilct with
	 * other region
	 */
	list_for_each_entry(region, &mem_list, list) {
		r_size = region->size;
		r_base = region->phy_base;
		r_end = r_base + r_size - 1;

		if (!((base > r_end) || ((base + size) < r_base))) {
			pr_err("memory region invalid 0x%p ---> [0x%p]\n",
					r_base, r_end, r_size);
			return -EINVAL;
		}
	}

	region = alloc_memory_region();
	region->phy_base = base;
	region->size = size;
	region->type = type;
	region->vmid = vmid;

	init_list(&region->list);
	list_add_tail(&mem_list, &region->list);
	pr_info("ADD   MEM: 0x%p [0x%p] 0x%x\n", region->phy_base,
			region->size, region->type);

	return 0;
}

int split_memory_region(uint64_t base, size_t size, int type, int vmid)
{
	phy_addr_t start, end;
	phy_addr_t new_end = base + size;
	struct memory_region *region, *n, *tmp;

	if ((size == 0) || (type >= MEMORY_REGION_TYPE_MAX))
		return -EINVAL;

	pr_info("SPLIT MEM: 0x%p [0x%p] 0x%x\n", base, size, type);

	/*
	 * delete the memory for host, these region
	 * usually for vms
	 */
	list_for_each_entry_safe(region, n, &mem_list, list) {
		start = region->phy_base;
		end = start + region->size;

		if ((base > end) || (base < start) || (new_end > end))
			continue;

		/* just delete this region from the list */
		if ((base == start) && (new_end == end)) {
			region->type = type;
			return 0;
		} else if ((base == start) && (new_end < end)) {
			region->phy_base = new_end;
			region->size -= size;
		} else if ((base > start) && (new_end < end)) {
			/* create a new region for the tail space */
			n = alloc_memory_region();
			n->phy_base = new_end;
			n->size = end - new_end;
			n->type = region->type;
			n->vmid = region->vmid;
			list_add_tail(&mem_list, &n->list);

			region->size = base - start;
		} else if ((base > start) && (end == new_end)) {
			region->size = region->size - size;
		} else {
			pr_warn("incorrect memory region 0x%x 0x%x\n",
					base, size);
			return -EINVAL;
		}

		/* alloc a new memory region for vm memory */
		tmp = alloc_memory_region();
		tmp->phy_base = base;
		tmp->size = size;
		tmp->type = type;
		tmp->vmid = vmid;
		list_add_tail(&mem_list, &tmp->list);

		return 0;
	}

	panic("Found Invalid memory config 0x%p [0x%p]\n", base, size);

	return 0;
}

void dump_memory_info(void)
{
	struct memory_region *region;
	char vm[8];

	char *mem_attr[MEMORY_REGION_TYPE_MAX] = {
		"Normal",
		"DMA",
		"RSV",
		"VM",
		"DTB",
		"Kernel",
		"RamDisk",
	};

	list_for_each_entry(region, &mem_list, list) {
		sprintf(vm, "VM%d", region->vmid);
		pr_notice("MEM: 0x%p -> 0x%p [0x%p] %s/%s\n", region->phy_base,
				region->phy_base + region->size,
				region->size, mem_attr[region->type],
				region->vmid == 0 ? "Host" : vm);
	}
}
