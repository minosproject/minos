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
#include <minos/tlb.h>

static struct mm_struct host_mm;

extern unsigned char __el2_page_table;

#define PHYSIC_ADDRESS_MASK	(0x0000ffffffffffff)

#define DESC_MASK(n)		(~((1UL << (n)) - 1))

#define PGD_DES_MASK		DESC_MASK(PGD_DES_OFFSET)
#define PUD_DES_MASK		DESC_MASK(PUD_DES_OFFSET)
#define PMD_DES_MASK		DESC_MASK(PMD_DES_OFFSET)
#define PTE_DES_MASK		DESC_MASK(PTE_DES_OFFSET)

#define pud_table_addr(pgd)	((pgd & PHYSIC_ADDRESS_MASK) & PGD_DES_MASK)
#define pmd_table_addr(pud)	((pud & PHYSIC_ADDRESS_MASK) & PUD_DES_MASK)
#define pte_table_addr(pmd)	((pmd & PHYSIC_ADDRESS_MASK) & PMD_DES_MASK)

#define ENTRY_MAP_SIZE(s, vaddr, ems)		\
	({					\
	size_t __ms;				\
	__ms = BALIGN(vaddr, ems) - vaddr;	\
	__ms = __ms ? __ms : ems;		\
	if (__ms > s)				\
		__ms = s;			\
	__ms; })

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

static uint64_t alloc_mapping_page(struct mm_struct *mm)
{
	struct page *page;

	page = alloc_page();
	if (!page)
		return 0;

	memset(page_to_addr(page), 0, PAGE_SIZE);
	page->next = mm->page_head;
	mm->page_head = page;

	return (uint64_t)(page_to_addr(page));
}

static int build_pte_entry(struct mm_struct *mm,
			   uint64_t *ptep,
			   vir_addr_t vaddr,
			   phy_addr_t paddr,
			   size_t size,
			   unsigned long flags)
{
	unsigned long offset;
	uint64_t attr = page_table_description(flags | VM_DES_PAGE);

	if (size > PMD_MAP_SIZE)
		return -EINVAL;

	while (size > 0) {
		if (flags & VM_HOST)
			offset = pte_idx(vaddr);
		else
			offset = guest_pte_idx(vaddr);

		*(ptep + offset) = attr | (paddr & PTE_DES_MASK);

		size -= PTE_MAP_SIZE;
		vaddr += PTE_MAP_SIZE;
		paddr += PTE_MAP_SIZE;
	}

	return 0;
}

static int build_pmd_entry(struct mm_struct *mm,
			   pmd_t *pmdp,
			   vir_addr_t vaddr,
			   phy_addr_t paddr,
			   size_t size,
			   unsigned long flags)
{
	pmd_t pmd;
	size_t map_size;
	unsigned long offset;
	int map_as_block = 0;
	uint64_t t_attr = page_table_description(flags | VM_DES_TABLE);
	uint64_t b_attr = page_table_description(flags | VM_DES_BLOCK);

	if (size > PUD_MAP_SIZE)
		return -EINVAL;

	/*
	 * one entry in PUD can map 2M memory, PUD can mapped
	 * as 2M block if need, if 2M block is enabled then need
	 * only need check whether the previous mapping type
	 */
	while (size > 0) {
		/*
		 * check whether this region can be mapped as block, if
		 * map as block, need check currently mapping type
		 */
		map_size = ENTRY_MAP_SIZE(size, vaddr, PMD_MAP_SIZE);
		if (flags & VM_HOST)
			offset = pmd_idx(vaddr);
		else
			offset = guest_pmd_idx(vaddr);

		if (IS_PMD_ALIGN(vaddr) && IS_PMD_ALIGN(paddr) &&
				(map_size == PMD_MAP_SIZE)) {
			pr_debug("0x%x--->0x%x @0x%x mapping as PMD block\n",
					vaddr, paddr, map_size);
			map_as_block = 1;
		}

		pmd = *(pmdp + offset);

		if (!pmd) {
			if (map_as_block) {
				*(pmdp + offset) = b_attr | (paddr & PMD_DES_MASK);
				goto repeat;
			} else {
				pmd = (pmd_t)alloc_mapping_page(mm);
				if (!pmd)
					return -ENOMEM;

				*(pmdp + offset) = t_attr | (pmd & PMD_DES_MASK);
			}
		} else if (pmd && map_as_block){
			pr_warn("PMD block remap 0x%x ---> 0x%x @0x%x\n",
					vaddr, paddr, map_size);
			*(pmdp + offset) = b_attr | (paddr & PMD_DES_MASK);
			goto repeat;
		} else {
			pmd = pmd_table_addr(pmd);
		}

		build_pte_entry(mm, (pte_t *)pmd,
				vaddr, paddr, map_size, flags);

repeat:
		size -= map_size;
		vaddr += map_size;
		paddr += map_size;
		map_as_block = 0;
	}

	return 0;
}

static int build_pud_entry(struct mm_struct *mm,
			   pud_t *pudp,
			   vir_addr_t vaddr,
			   phy_addr_t paddr,
			   size_t size,
			   unsigned long flags)
{
	pud_t pud;
	size_t map_size;
	unsigned long offset;
	int map_as_block = 0;
	uint64_t t_attr = page_table_description(flags | VM_DES_TABLE);
	uint64_t b_attr = page_table_description(flags | VM_DES_BLOCK);

	/*
	 * one entry in PUD can map 1G  memory, PUD can mapped
	 * as 1G block if need, if 1G block is enabled then need
	 * only need check whether the previous mapping type
	 */
	while (size > 0) {
		map_size = ENTRY_MAP_SIZE(size, vaddr, PUD_MAP_SIZE);
		if (flags & VM_HOST)
			offset = pud_idx(vaddr);
		else
			offset = guest_pud_idx(vaddr);

		/*
		 * check whether this region can be mapped as block, if
		 * map as block, need check currently mapping type
		 *
		 * for hypervisor itself, only support pmd block, since
		 * at boot stage will allocate 4pages for PUD, so each
		 * PUD will mapped to pmd block
		 *
		 */
		if (IS_PUD_ALIGN(vaddr) && IS_PUD_ALIGN(paddr) &&
				(map_size == PUD_MAP_SIZE) &&
				!(flags & VM_HOST)) {
			pr_debug("0x%x--->0x%x @0x%x mapping as PUD block\n",
					vaddr, paddr, map_size);
			map_as_block = 1;
		}

		/*
		 * should need to support dynamic mapping? here do not
		 * allowed to change the mapping entry, since:
		 * 1 - for host, the mapping is point to point
		 * 2 - for guest, the mapping will managed by vmm_area
		 *
		 * so if pud table has been already create but the region
		 * can be mapped as block, means there are someting error
		 */
		pud = *(pudp + offset);

		if (!pud) {
			if (map_as_block) {
				*(pudp + offset) = b_attr | (paddr & PUD_DES_MASK);
				goto repeat;
			} else {
				pud = (pud_t)alloc_mapping_page(mm);
				if (!pud)
					return -ENOMEM;

				*(pudp + offset) = t_attr | (pud & PUD_DES_MASK);
			}
		} else if (pud && map_as_block){
			pr_warn("PUD block remap 0x%x ---> 0x%x @0x%x\n",
					vaddr, paddr, map_size);
			*(pudp + offset) = b_attr | (paddr & PUD_DES_MASK);
			goto repeat;
		} else {
			pud = pmd_table_addr(pud);
		}

		build_pmd_entry(mm, (pmd_t *)pud,
				vaddr, paddr, map_size, flags);

repeat:
		size -= map_size;
		vaddr += map_size;
		paddr += map_size;
		map_as_block = 0;
	}

	return 0;
}

static int build_pgd_entry(struct mm_struct *mm,
			   pgd_t *pgdp,
			   vir_addr_t vaddr,
			   phy_addr_t paddr,
			   size_t size,
			   unsigned long flags)
{
	pgd_t pgd;
	size_t map_size;
	unsigned long offset;
	uint64_t attr = page_table_description(flags | VM_DES_TABLE);

	/*
	 * step 1: get the mapping size
	 * step 2: check whether can mapped as block
	 * step 3: check whether the entry has been aready mapped
	 *
	 * for block mapping - only support 1G 2M block mapping
	 *
	 * 4 level mapping table, one entry in PGD can map 512G
	 * memory
	 */
	while (size > 0) {
		map_size = ENTRY_MAP_SIZE(size, vaddr, PGD_MAP_SIZE);

		if (flags & VM_HOST)
			offset = pgd_idx(vaddr);
		else
			offset = guest_pgd_idx(vaddr);

		pgd = *(pgdp + offset);
		if (!pgd) {
			pgd = (pgd_t)alloc_mapping_page(mm);
			if (!pgd)
				return -ENOMEM;

			*(pgdp + offset) = attr | (pgd & PGD_DES_MASK);
		} else
			pgd = pud_table_addr(pgd);

		build_pud_entry(mm, (pud_t *)pgd, vaddr,
				paddr, map_size, flags);

		size -= map_size;
		vaddr += map_size;
		paddr += map_size;
	}

	return 0;
}

static int inline create_table_entry(struct mm_struct *mm,
				     uint64_t *tb,
				     vir_addr_t vaddr,
				     phy_addr_t paddr,
				     size_t size,
				     unsigned long flags)
{
	if (flags & VM_HOST) {
		return build_pgd_entry(mm, (pgd_t *)tb,
				vaddr, paddr, size, flags);
	} else {
		return build_pud_entry(mm, (pud_t *)tb,
				vaddr, paddr, size, flags);
	}
}

int create_mem_mapping(struct mm_struct *mm, vir_addr_t addr,
		phy_addr_t phy, size_t size, unsigned long flags)
{
	int ret;

	spin_lock(&mm->mm_lock);

	ret = create_table_entry(mm, (uint64_t *)mm->pgd_base,
			addr, phy, size, flags);

	spin_unlock(&mm->mm_lock);

	if (ret)
		pr_err("map fail 0x%x->0x%x size:%x\n", addr, phy, size);

	return ret;
}

static phy_addr_t mmu_translate_address(struct mapping_struct *info)
{
	int type, lvl = info->lvl;
	unsigned long des, offset;
	vir_addr_t va = info->vir_base;
	struct pagetable_attr *attr = info->config;
	unsigned long *table = (unsigned long *)info->table_base;

	do {
		offset = (va & attr->offset_mask) >> attr->range_offset;
		des = *(table + offset);
		if (0 == des)
			return 0;

		type = get_mapping_type(lvl, des);
		if (type == VM_DES_FAULT)
			return 0;

		if (type == VM_DES_TABLE) {
			lvl++;
			table = (unsigned long *)(des & VM_ADDRESS_MASK);
			attr = attr->next;
			if ((lvl > PTE) || (attr == NULL))
				return 0;
		} else {
			return (des & VM_ADDRESS_MASK);
		}
	} while (1);
}

phy_addr_t mmu_translate_guest_address(void *pgd, unsigned long va)
{
	struct mapping_struct info;

	memset(&info, 0, sizeof(info));
	info.table_base = (unsigned long)pgd;
	info.vir_base = va;
	info.lvl = PUD;
	info.config = attrs[PUD];

	return mmu_translate_address(&info);
}

phy_addr_t mmu_translate_host_address(void *pgd, unsigned long va)
{
	struct mapping_struct info;

	memset(&info, 0, sizeof(info));
	info.table_base = (unsigned long)pgd;
	info.vir_base = va;
	info.lvl = PGD;
	info.config = attrs[PGD];

	return mmu_translate_address(&info);
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
				pr_err("mapping error on 0x%x\n", vir);
				return -EINVAL;
			}

			type = get_mapping_type(lvl, des);
			if (type == VM_DES_FAULT)
				return -EINVAL;

			if (type == VM_DES_TABLE) {
				lvl++;
				table = (unsigned long *)(des & ~PAGE_MASK);
				attr = attr->next;
				if ((lvl > PTE) || (attr == NULL)) {
					pr_err("mapping error on 0x%x\n", vir);
					return -EINVAL;
				}
			} else {
				/* check the size with the talbe mapping size */
				if (size < attr->map_size) {
					pr_err("can not destroy mapping 0x%p [0x%x] @0x%x entry\n",
							vir, size, attr->map_size);
					return -EINVAL;
				}
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

	map_info.table_base = mm->pgd_base;
	map_info.vir_base = vir;
	map_info.size = size;

	if (flags & VM_HOST) {
		map_info.lvl = PGD;
		map_info.config = attrs[PGD];
	} else {
		map_info.lvl = PUD;
		map_info.config = attrs[PUD];
	}

	spin_lock(&mm->mm_lock);
	__destroy_mem_mapping(&map_info);
	spin_unlock(&mm->mm_lock);

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
		table = (unsigned long *)(value & 0x0000fffffffff000);
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
	spin_lock(&mm->mm_lock);
	pmd = alloc_mapping_page(mm);
	if (pmd)
		create_pud_mapping(mm->pgd_base, phy, pmd, VM_DES_TABLE);

	spin_unlock(&mm->mm_lock);
	return pmd;
}

int create_host_mapping(vir_addr_t vir, phy_addr_t phy,
		size_t size, unsigned long flags)
{
	size_t new_size;
	unsigned long vir_base, phy_base, tmp;

	/*
	 * for host mapping, IO and Normal memory all mapped
	 * as PAGE_SIZE ALIGN
	 */
	tmp = BALIGN(vir + size, PAGE_SIZE);
	vir_base = ALIGN(vir, PAGE_SIZE);
	phy_base = ALIGN(phy, PAGE_SIZE);
	new_size = tmp - vir_base;

	flags |= VM_HOST;

	return create_mem_mapping(&host_mm,
			vir_base, phy_base, new_size, flags);
}

int destroy_host_mapping(vir_addr_t vir, size_t size)
{
	int ret;
	unsigned long end;

	if (!IS_PMD_ALIGN(vir) || !IS_PMD_ALIGN(size)) {
		pr_warn("destroy host mapping 0x%x---->0x%x\n",
				vir, vir + size);
	}

	end = BALIGN(vir + size, MEM_BLOCK_SIZE);
	vir = ALIGN(vir, MEM_BLOCK_SIZE);
	size = end - vir;

	ret = destroy_mem_mapping(&host_mm, vir, size, VM_HOST);
	flush_tlb_va_host(vir, size);

	return ret;
}

unsigned long io_remap(vir_addr_t vir, size_t size)
{
	if (!create_host_mapping(vir, vir, size, VM_IO))
		return vir;
	else
		return 0;
}

void io_unmap(unsigned long vir, size_t size)
{
	destroy_host_mapping(vir, size);
}

static int __init_text vmm_early_init(void)
{
	spin_lock_init(&host_mm.mm_lock);
	host_mm.pgd_base = (unsigned long)&__el2_page_table;

	return 0;
}
early_initcall(vmm_early_init);
