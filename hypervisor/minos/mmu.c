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
#include <minos/vcpu.h>

#define DESC_MASK(n) (~((1UL << (n)) - 1))

struct pagetable_attr {
	int range_offset;
	int des_offset;
	int lvl;
	unsigned long offset_mask;
	size_t map_size;
	struct pagetable_attr *next;
};

struct pagetable_attr pte_attr = {
	.range_offset 	= PTE_RANGE_OFFSET,
	.des_offset 	= PTE_DES_OFFSET,
	.lvl		= PTE,
	.offset_mask	= PTE_ENTRY_OFFSET_MASK,
	.map_size	= PTE_MAP_SIZE,
	.next		= NULL,
};

struct pagetable_attr pmd_attr = {
	.range_offset 	= PMD_RANGE_OFFSET,
	.des_offset 	= PMD_DES_OFFSET,
	.lvl		= PMD,
	.offset_mask	= PMD_ENTRY_OFFSET_MASK,
	.map_size	= PMD_MAP_SIZE,
	.next		= &pte_attr,
};

struct pagetable_attr pud_attr = {
	.range_offset 	= PUD_RANGE_OFFSET,
	.des_offset 	= PUD_DES_OFFSET,
	.lvl		= PUD,
	.offset_mask	= PUD_ENTRY_OFFSET_MASK,
	.map_size	= PUD_MAP_SIZE,
	.next		= &pmd_attr,
};

struct pagetable_attr pgd_attr = {
	.range_offset 	= PGD_RANGE_OFFSET,
	.des_offset 	= PGD_DES_OFFSET,
	.lvl		= PGD,
	.offset_mask	= PGD_ENTRY_OFFSET_MASK,
	.map_size	= PGD_MAP_SIZE,
	.next		= &pud_attr,
};

static struct pagetable_attr *attrs[PTE + 1] = {
	&pgd_attr,
	&pud_attr,
	&pmd_attr,
	&pte_attr,
};

unsigned long get_tt_description(int host, int m_type, int d_type)
{
	if (host)
		return arch_host_tt_description(m_type, d_type);
	else
		return arch_guest_tt_description(m_type, d_type);
}

static int get_map_type(struct mapping_struct *info)
{
	struct pagetable_attr *config = info->config;
	unsigned long a, b;

	if (info->host) {
		if (info->lvl == PMD)
			return DESCRIPTION_BLOCK;
	} else {
		if (info->lvl == PTE)
			return DESCRIPTION_PAGE;
	}

	/*
	 * check whether the map size is level map
	 * size align and the virtual base aligin
	 * FIX ME : whether need to check the physical base?
	 */
	a = (info->size) & (config->map_size - 1);
	b = (info->vir_base) & (config->map_size - 1);
	if (a || b)
		return DESCRIPTION_TABLE;
	else
		return DESCRIPTION_BLOCK;
}

static int create_page_entry(struct mapping_struct *info)
{
	int i, map_type;
	uint32_t offset, index;
	uint64_t attr;
	struct pagetable_attr *config = info->config;
	unsigned long *tbase = (unsigned long *)info->table_base;

	if (info->lvl != 3)
		map_type = DESCRIPTION_BLOCK;
	else
		map_type = DESCRIPTION_PAGE;

	offset = config->range_offset;
	attr = get_tt_description(info->host, info->mem_type, map_type);

	index = (info->vir_base & config->offset_mask) >> offset;

	for (i = 0; i < (info->size >> offset); i++) {
		*(tbase + index) = attr | (info->phy_base &
				DESC_MASK(config->des_offset));
		info->vir_base += config->map_size;
		info->phy_base += config->map_size;
		index++;
	}

	return 0;
}

static int create_table_entry(struct mapping_struct *info)
{
	size_t size, map_size;
	unsigned long attr;
	unsigned long value, offset;
	int ret = 0, map_type, new_page;
	struct mapping_struct map_info;
	struct pagetable_attr *config = info->config;
	unsigned long *tbase = (unsigned long *)info->table_base;

	size = info->size;
	attr = get_tt_description(info->host,
			info->mem_type, DESCRIPTION_TABLE);

	while (size > 0) {
		new_page = 0;
		map_size = BALIGN(info->vir_base, config->map_size) - info->vir_base;
		map_size = map_size ? map_size : config->map_size;
		if (map_size > size)
			map_size = size;

		offset = (info->vir_base & config->offset_mask) >>
			config->range_offset;
		value = *(tbase + offset);

		if (!value) {
			value = (unsigned long)
				info->get_free_pages(1, info->data);
			if (!value)
				return -ENOMEM;

			new_page = 1;
			memset((void *)value, 0, SIZE_4K);
			*(tbase + offset) = attr | (value &
					DESC_MASK(config->des_offset));
			flush_dcache_range((unsigned long)(tbase + offset),
						sizeof(unsigned long));
		} else {
			/* get the base address of the entry */
			value = value & 0x0000ffffffffffff;
			value = value >> config->des_offset;
			value = value << config->des_offset;
		}

		memset(&map_info, 0, sizeof(struct mapping_struct));
		map_info.table_base = value;
		map_info.vir_base = info->vir_base;
		map_info.phy_base = info->phy_base;
		map_info.size = map_size;
		map_info.lvl = info->lvl + 1;
		map_info.mem_type = info->mem_type;
		map_info.host = info->host;
		map_info.data = info->data;
		map_info.config = config->next;
		map_info.get_free_pages = info->get_free_pages;

		/*
		 * get next level map entry type, if the entry
		 * has been already maped then force it to a
		 * Table description
		 *
		 * FIX ME: if the attribute of the page table entry
		 * is changed, such as from TABLE to BLOCK, need to
		 * free the page table page --- TBD
		 */
		map_type = get_map_type(&map_info);

		if (map_type == DESCRIPTION_TABLE)
			ret = create_table_entry(&map_info);
		else
			ret = create_page_entry(&map_info);
		if (ret) {
			if (new_page) {
				free((void *)value);
				new_page = 0;
			}

			return ret;
		}

		flush_dcache_range(value, SIZE_4K);
		info->vir_base += map_size;
		size -= map_size;
		info->phy_base += map_size;
	}

	return ret;
}

int create_mem_mapping(struct mapping_struct *info)
{
	int ret;

	if (!info->config)
		info->config = attrs[info->lvl];

	ret = create_table_entry(info);
	if (ret)
		pr_error("map host 0x%x->0x%x size:%x failed\n",
				info->vir_base, info->phy_base, info->size);

	if (info->host)
		flush_all_tlb_host();
	else
		flush_local_tlb_guest();

	return ret;
}

static int __destory_mem_mapping(struct mapping_struct *info)
{
	int type;
	int lvl = info->lvl;
	unsigned long *table;
	long size = (long)info->size;
	unsigned long des, offset;
	unsigned long vir = info->vir_base;
	struct pagetable_attr *attr;

	while (size > PAGE_SIZE) {
		table = (unsigned long *)info->table_base;
		attr = info->config;
		do {
			offset = (vir & attr->offset_mask) >> attr->range_offset;
			des = *(table + offset);
			if (des == 0) {
				pr_error("mapping error on 0x%x\n", vir);
				return -EINVAL;
			}

			type = get_mapping_type(lvl, des);
			if (type == DESCRIPTION_FAULT)
				return -EINVAL;

			if (type == DESCRIPTION_TABLE) {
				lvl++;
				table = (unsigned long *)des;
				attr = attr->next;
				if ((lvl > PTE) || (attr == NULL)) {
					pr_error("mapping error on 0x%x\n", vir);
					return -EINVAL;
				}
			} else {
				*(table + offset) = 0;
				size -= attr->map_size;
				vir += attr->map_size;
				flush_dcache_range((unsigned long)(table + offset),
						sizeof(unsigned long));
				break;
			}
		} while (1);
	}

	return 0;
}

int destory_mem_mapping(struct mapping_struct *info)
{
	if (!info->config)
		info->config = attrs[info->lvl];

	__destory_mem_mapping(info);

	if (info->host)
		flush_all_tlb_host();
	else
		flush_local_tlb_guest();

	return 0;
}

unsigned long get_mapping_entry(unsigned long tt,
		unsigned long vir, int start, int end)
{
	unsigned long offset;
	unsigned long value;
	unsigned long *table = (unsigned long *)tt;
	struct pagetable_attr *attr = attrs[start];

	do {
		offset = (vir & attr->offset_mask) >> attr->range_offset;
		value = *(table + offset);
		if ((value == 0) && (attr->lvl < end))
			return attr->lvl;

		attr = attr->next;
		table = (unsigned long *)(value & 0xfffffffffffff000);
	} while (attr->lvl < end);

	return (unsigned long)table;
}

void create_level_mapping(int lvl, unsigned long tt, unsigned long addr,
			int mem_type, int map_type, int host)
{
	unsigned long attr;
	unsigned long offset;
	struct pagetable_attr *ar = attrs[lvl];

	//offset = addr >> ar->range_offset;
	attr = get_tt_description(host, mem_type, map_type);

	*(unsigned long *)(tt) =
		(addr & 0xfffffffffffff000) | attr;
}
