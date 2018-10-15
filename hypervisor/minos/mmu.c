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

unsigned long page_table_description(unsigned long flags)
{
	return arch_page_table_description(flags);
}

static int get_map_type(struct mapping_struct *info)
{
	struct pagetable_attr *config = info->config;
	unsigned long a, b;

	if (info->flags & VM_HOST) {
		if (info->lvl == PMD)
			return VM_DES_BLOCK;
	} else {
		if (info->lvl == PTE)
			return VM_DES_PAGE;
	}

	/*
	 * check whether the map size is level map
	 * size align and the virtual base aligin
	 * FIX ME : whether need to check the physical base?
	 */
	a = (info->size) & (config->map_size - 1);
	b = (info->vir_base) & (config->map_size - 1);
	if (a || b)
		return VM_DES_TABLE;
	else
		return VM_DES_BLOCK;
}

static unsigned long alloc_mapping_page(struct mm_struct *mm)
{
	struct page *page;

	page = alloc_page();
	if (!page)
		return 0;

	memset(page_to_addr(page), 0, PAGE_SIZE);
	page->next = mm->head;
	mm->head = page;

	return (unsigned long)(page_to_addr(page));
}

static int create_page_entry(struct mm_struct *mm,
		struct mapping_struct *info)
{
	int i, map_type;
	uint32_t offset, index;
	uint64_t attr;
	struct pagetable_attr *config = info->config;
	unsigned long *tbase = (unsigned long *)info->table_base;

	if (info->lvl != 3)
		map_type = VM_DES_BLOCK;
	else
		map_type = VM_DES_PAGE;

	offset = config->range_offset;
	attr = page_table_description(info->flags | map_type);

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

static int create_table_entry(struct mm_struct *mm,
		struct mapping_struct *info)
{
	size_t size, map_size;
	unsigned long attr;
	unsigned long value, offset;
	int ret = 0, map_type, new_page;
	struct mapping_struct map_info;
	struct pagetable_attr *config = info->config;
	unsigned long *tbase = (unsigned long *)info->table_base;

	size = info->size;
	attr = page_table_description(info->flags | VM_DES_TABLE);

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
			value = alloc_mapping_page(mm);
			if (!value)
				return -ENOMEM;

			new_page = 1;
			memset((void *)value, 0, PAGE_SIZE);
			*(tbase + offset) = attr | (value &
					DESC_MASK(config->des_offset));
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
		map_info.flags = info->flags;
		map_info.config = config->next;

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

		if (map_type == VM_DES_TABLE)
			ret = create_table_entry(mm, &map_info);
		else
			ret = create_page_entry(mm, &map_info);
		if (ret) {
			if (new_page) {
				free((void *)value);
				new_page = 0;
			}

			return ret;
		}

		info->vir_base += map_size;
		size -= map_size;
		info->phy_base += map_size;
	}

	return ret;
}

int create_mem_mapping(struct mm_struct *mm, unsigned long addr,
		unsigned long phy, size_t size, unsigned long flags)
{
	int ret;
	struct mapping_struct map_info;

	memset(&map_info, 0, sizeof(struct mapping_struct));
	map_info.table_base = mm->pgd_base;
	map_info.vir_base = addr;
	map_info.phy_base = phy;
	map_info.size = size;
	map_info.flags = flags;

	if (flags & VM_HOST) {
		map_info.lvl = PGD;
		map_info.config = attrs[PGD];
	} else {
		map_info.lvl = PUD;
		map_info.config = attrs[PUD];
	}

	spin_lock(&mm->lock);
	ret = create_table_entry(mm, &map_info);
	spin_unlock(&mm->lock);

	if (ret)
		pr_error("map fail 0x%x->0x%x size:%x\n", addr, phy, size);

	/* need to flush the addr + size's mem's cache ? */
	if (flags & VM_HOST)
		flush_tlb_va_host(addr, size);
	else
		flush_local_tlb_guest();

	return ret;
}

static int __destroy_mem_mapping(struct mapping_struct *info)
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
			if (type == VM_DES_FAULT)
				return -EINVAL;

			if (type == VM_DES_TABLE) {
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
				break;
			}
		} while (1);
	}

	return 0;
}

int destroy_mem_mapping(struct mm_struct *mm, unsigned long vir,
		size_t size, unsigned long flags)
{
	struct mapping_struct map_info;

	memset(&map_info, 0, sizeof(struct mapping_struct));
	map_info.table_base = mm->pgd_base;
	map_info.vir_base = vir;
	map_info.lvl = PGD;
	map_info.size = size;
	map_info.config = attrs[PGD];

	spin_lock(&mm->lock);
	__destroy_mem_mapping(&map_info);
	spin_unlock(&mm->lock);

	if (flags & VM_HOST)
		flush_tlb_va_host(vir, size);
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
			return 0;

		attr = attr->next;
		table = (unsigned long *)(value & 0xfffffffffffff000);
	} while (attr->lvl < end);

	return (unsigned long)table;
}

static void create_level_mapping(int lvl, unsigned long tt,
		unsigned long vir, unsigned long addr, unsigned long flags)
{
	unsigned long attr;
	unsigned long offset;
	struct pagetable_attr *ar = attrs[lvl];

	offset = vir >> ar->range_offset;
	attr = page_table_description(flags);

	*((unsigned long *)(tt) + offset) =
		(addr & 0xfffffffffffff000) | attr;
}

void create_pgd_mapping(unsigned long pgd, unsigned long vir,
		unsigned long value, unsigned long flags)
{
	create_level_mapping(PGD, pgd, vir, value, flags);
}

void create_pud_mapping(unsigned long pud, unsigned long vir,
		unsigned long value, unsigned long flags)
{
	create_level_mapping(PUD, pud, vir, value, flags);
}

void create_pmd_mapping(unsigned long pmd, unsigned long vir,
		unsigned long value, unsigned long flags)
{
	create_level_mapping(PMD, pmd, vir, value, flags);
}

void create_pte_mapping(unsigned long pte, unsigned long vir,
		unsigned long value, unsigned long flags)
{
	create_level_mapping(PTE, pte, vir, value, flags);
}

unsigned long alloc_guest_pmd(struct mm_struct *mm, unsigned long phy)
{
	unsigned long pmd;

	if (!mm->pgd_base)
		return 0;

	pmd = get_mapping_entry(mm->pgd_base, phy, PUD, PMD);
	if (pmd)
		return pmd;

	/* alloc a new pmd mapping page */
	spin_lock(&mm->lock);
	pmd = alloc_mapping_page(mm);
	if (pmd)
		create_pud_mapping(mm->pgd_base, phy, pmd, VM_DES_TABLE);

	spin_unlock(&mm->lock);
	return pmd;
}

/*
 * create pmd mapping mapping 2m mem each time
 * used to early mapping
 */
int create_early_pmd_mapping(unsigned long vir, unsigned long phy)
{
	pud_t *pudp;
	pmd_t *pmdp;
	extern unsigned char __el2_ttb0_pgd;
	pgd_t *pgdp = (pgd_t *)&__el2_ttb0_pgd;

	/*
	 * create the early mapping for hypervisor
	 * will map phy to phy mapping, the pages needed
	 * for this function will used bootmem
	 */
	pudp = (pud_t *)(*pgd_offset(pgdp, vir) & ~PAGE_MASK);
	if (!pudp) {
		pudp = (pud_t *)alloc_boot_pages(1);
		if (!pudp)
			return -ENOMEM;

		memset(pudp, 0, PAGE_SIZE);
		set_pgd_at(pgd_offset(pgdp, vir), (unsigned long)pudp |
				VM_DESC_HOST_TABLE);
	}

	pmdp = (pmd_t *)(*pud_offset(pudp, vir) & ~PAGE_MASK);
	if (!pmdp) {
		pmdp = (pmd_t *)alloc_boot_pages(1);
		if (!pmdp)
			return -ENOMEM;

		memset(pmdp, 0, PAGE_SIZE);
		set_pud_at(pud_offset(pudp, vir), (unsigned long)pmdp |
				VM_DESC_HOST_TABLE);

	}

	set_pmd_at(pmd_offset(pmdp, phy), (phy & PMD_MASK) |
			VM_DESC_HOST_BLOCK);
	flush_tlb_va_host(vir, SIZE_2M);

	return 0;
}
