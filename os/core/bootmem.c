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
#include <minos/mm.h>

void *bootmem_start;
void *bootmem_end;
unsigned long bootmem_size = 0;
static DEFINE_SPIN_LOCK(bootmem_lock);
void *bootmem_page_base;

LIST_HEAD(mem_list);

#define BOOTMEM_MIN_SIZE	(32 * 1024)

void *alloc_boot_mem(size_t size)
{
	size_t request_size;
	void *base = NULL;

	request_size = BALIGN(size, sizeof(unsigned long));

	spin_lock(&bootmem_lock);
	if (bootmem_size < request_size)
		panic("no more memory or mm has been init\n");
	else
		base = bootmem_start;

	bootmem_start += request_size;
	bootmem_size -= request_size;
	spin_unlock(&bootmem_lock);

	return base;
}

void *alloc_boot_pages(int pages)
{
	void *base = NULL;
	size_t request_size = pages * PAGE_SIZE;

	if (unlikely(pages <= 0))
		return NULL;

	spin_lock(&bootmem_lock);
	if (bootmem_size < request_size)
		panic("no more memory or mm has been init\n");

	if (bootmem_page_base -
			request_size < bootmem_start)
		panic("no more memory or mm has been init\n");

	base = bootmem_page_base - request_size;
	bootmem_page_base = base;
	bootmem_size -= request_size;
	spin_unlock(&bootmem_lock);

	return base;
}

void bootmem_init(void)
{
	size_t min_size = 0, size;
	extern unsigned char __code_end;
	extern unsigned char __code_start;

#ifdef CONFIG_BOOTMEM_SIZE
	min_size = CONFIG_BOOTMEM_SIZE;
	if (min_size < BOOTMEM_MIN_SIZE)
		min_size = BOOTMEM_MIN_SIZE;
#else
	min_size = BOOTMEM_MIN_SIZE;
#endif

	size = (&__code_end) - (&__code_start);
	size = PAGE_BALIGN(size);

	if ((size + min_size) >= CONFIG_MINOS_RAM_SIZE)
		panic("no more memory for bootmem\n");

	size = CONFIG_MINOS_RAM_SIZE - size;
#ifndef CONFIG_SIMPLE_MM_ALLOCATER
	min_size = min_size > size ? size : min_size;
#else
	min_size = size;
#endif

	bootmem_start = (void *)BALIGN((unsigned long)&__code_end,
			sizeof(unsigned long));
	bootmem_end = (void *)PAGE_BALIGN((unsigned long)&__code_end
			+ min_size);
	bootmem_size = bootmem_end - bootmem_start;
	bootmem_page_base = bootmem_end;

	pr_info("bootmem start-0x%p end-0x%p size-0x%x\n",
			(unsigned long)bootmem_start,
			(unsigned long)bootmem_end,
			(unsigned long)bootmem_size);
}

int add_memory_region(uint64_t base, uint64_t size, uint32_t flags)
{
	struct memory_region *region;

	if (size == 0)
		return -EINVAL;

	region = alloc_boot_mem(sizeof(struct memory_region));
	if (!region)
		panic("no more boot memory\n");

	memset((void *)region, 0, sizeof(struct memory_region));
	region->vir_base = base;
	region->phy_base = base;
	region->size = size;
	region->flags = flags;

	pr_info("ADD MEM : 0x%x -> 0x%x 0x%x %d\n", region->vir_base,
		region->phy_base, region->size, region->flags);

	init_list(&region->list);
	list_add_tail(&mem_list, &region->list);

	return 0;
}

int split_memory_region(vir_addr_t base, size_t size, uint32_t flags)
{
	vir_addr_t start, end;
	vir_addr_t new_end = base + size;
	struct memory_region *region, *n, *tmp;

	pr_info("SPLIT MEM 0x%x 0x%x\n", base, size);

	if ((size == 0))
		return -EINVAL;

	/*
	 * delete the memory for host, these region
	 * usually for vms
	 */
	list_for_each_entry_safe(region, n, &mem_list, list) {
		start = region->vir_base;
		end = start + region->size;

		if ((base > end) || (base < start) || (new_end > end))
			continue;

		/* just delete this region from the list */
		if ((base == start) && (new_end == end)) {
			return 0;
		} else if ((base == start) && (new_end < end)) {
			region->vir_base = region->phy_base = new_end;
			region->size -= size;
		} else if ((base > start) && (new_end < end)) {
			n = alloc_boot_mem(sizeof(struct memory_region));
			if (!n)
				panic("no more boot memory\n");
			init_list(&n->list);
			n->vir_base = n->phy_base = new_end;
			n->size = end - new_end;
			n->flags = region->flags;
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
		tmp = alloc_boot_mem(sizeof(struct memory_region));
		if (!tmp)
			panic("no more boot memory\n");
		init_list(&tmp->list);
		tmp->vir_base = tmp->phy_base = base;
		tmp->size = size;
		tmp->flags = region->flags;
		list_add_tail(&mem_list, &tmp->list);

		return 0;
	}

	add_memory_region(base, size, flags);
	return 0;
}
