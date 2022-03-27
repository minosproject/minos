/*
 * Copyright (C) 2020 Min Le (lemin9538@gmail.com)
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
#include <minos/vspace.h>
#include <asm/tlb.h>
#include <asm/cache.h>
#include "stage1.h"

/*
 * 40bit stage1 IPA use 3 levels page table, the pagetable
 * need 2 pages
 */
#define S1_PGTABLE_LEVELS		3
#define S1_PAGETABLE_SIZE		4096

#define S1_PGD_SHIFT			39
#define S1_PGD_SIZE			(1UL << S1_PGD_SHIFT)
#define S1_PGD_MASK			(~(S1_PGD_SIZE - 1))

#define S1_PUD_SHIFT			30
#define S1_PUD_SIZE			(1UL << S1_PUD_SHIFT)
#define S1_PUD_MASK			(~(S1_PUD_SIZE - 1))

#define S1_PMD_SHIFT			21
#define S1_PMD_SIZE			(1UL << S1_PMD_SHIFT)
#define S1_PMD_MASK			(~(S1_PMD_SIZE - 1))

#define S1_PTE_SHIFT			12
#define S1_PTE_SIZE			(1UL << S1_PTE_SHIFT)
#define S1_PTE_MASK			(~(S1_PTE_SIZE - 1))

#define S1_PHYSICAL_MASK		0x0000fffffffff000UL
#define S1_PHYSICAL_MAX			(1UL << 40)
#define S1_VIRT_MAX			(1UL << 39)

/*
 * The number of PTRS across all concatenated stage1 tables given by the
 * number of bits resolved at the initial level.
 *
 * since we use 3 level pagetable, and translation walk will from pud, the
 * PTRS in one pud is 1024
 */
#define PTRS_PER_S1_PGD			512
#define PTRS_PER_S1_PUD			512
#define PTRS_PER_S1_PMD			512
#define PTRS_PER_S1_PTE			512

#define stage1_pud_value(pudp)		(*(pudp))
#define stage1_pmd_value(pmdp)		(*(pmdp))
#define stage1_pte_value(ptep)		(*(ptep))

#define stage1_pgd_index(addr)		((((addr) & S1_PHYSICAL_MASK) >> S1_PGD_SHIFT) & (PTRS_PER_S1_PGD - 1))
#define stage1_pud_index(addr)		((((addr) & S1_PHYSICAL_MASK) >> S1_PUD_SHIFT) & (PTRS_PER_S1_PUD - 1))
#define stage1_pmd_index(addr)		((((addr) & S1_PHYSICAL_MASK) >> S1_PMD_SHIFT) & (PTRS_PER_S1_PMD - 1))
#define stage1_pte_index(addr)		((((addr) & S1_PHYSICAL_MASK) >> S1_PTE_SHIFT) & (PTRS_PER_S1_PTE - 1))

#define stage1_pgd_offset(pgdp, addr)		((pgd_t *)(pgdp) + stage1_pgd_index((unsigned long)addr))
#define stage1_pud_offset(pudp, addr)		((pud_t *)(pudp) + stage1_pud_index((unsigned long)addr))
#define stage1_pmd_offset(pmdp, addr)		((pmd_t *)(pmdp) + stage1_pmd_index((unsigned long)addr))
#define stage1_pte_offset(ptep, addr)		((pte_t *)(ptep) + stage1_pte_index((unsigned long)addr))

#define stage1_pud_huge(pud)			((pud) && ((pud) & 0x03) == S1_DES_BLOCK)
#define stage1_pmd_huge(pmd)			((pmd) && ((pmd) & 0x03) == S1_DES_BLOCK)

#define stage1_pud_table_addr(pgd)		(pud_t *)(unsigned long)((pgd) & S1_PHYSICAL_MASK)
#define stage1_pmd_table_addr(pud)		(pmd_t *)(unsigned long)((pud) & S1_PHYSICAL_MASK)
#define stage1_pte_table_addr(pmd)		(pte_t *)(unsigned long)((pmd) & S1_PHYSICAL_MASK)

#define stage1_pgd_none(pgd)			((pgd) == 0)
#define stage1_pud_none(pud)			((pud) == 0)
#define stage1_pmd_none(pmd)			((pmd) == 0)
#define stage1_pte_none(pte)			((pte) == 0)

#define stage1_phy_pte(pte)			(void *)((pte) & S1_PHYSICAL_MASK)
#define stage1_phy_pmd(pmd)			(void *)((pmd) & S1_PHYSICAL_MASK)

static void inline flush_tlb_va_range(unsigned long va, size_t size)
{
	flush_tlb_va_host(va, size);
}

static inline void flush_dcache_pte(unsigned long addr)
{
	flush_dcache_range(addr, PAGE_SIZE);
}

static inline void flush_dcache_pmd(unsigned long addr)
{
	flush_dcache_range(addr, S1_PMD_SIZE);
}

static void inline stage1_pgd_clear(pud_t *pgdp)
{
	WRITE_ONCE(*pgdp, 0);
	__dsb(ishst);
	isb();
}

static void inline stage1_pud_clear(pud_t *pudp)
{
	WRITE_ONCE(*pudp, 0);
	__dsb(ishst);
	isb();
}

static void inline stage1_pmd_clear(pmd_t *pmdp)
{
	WRITE_ONCE(*pmdp, 0);
	__dsb(ishst);
	isb();
}

static void *stage1_get_free_page(unsigned long flags)
{
	return get_free_page();
}

static unsigned long stage1_xxx_addr_end(unsigned long start, unsigned long end, size_t map_size)
{
	unsigned long boundary = (start + map_size) & ~((unsigned long)map_size - 1);

	return ((boundary - 1) < (end - 1)) ? boundary : end;
}

#define stage1_pgd_addr_end(start, end)	\
	stage1_xxx_addr_end(start, end, S1_PGD_SIZE)

#define stage1_pud_addr_end(start, end)	\
	stage1_xxx_addr_end(start, end, S1_PUD_SIZE)

#define stage1_pmd_addr_end(start, end)	\
	stage1_xxx_addr_end(start, end, S1_PMD_SIZE)

static inline void stage1_set_pte(pte_t *ptep, pte_t new_pte)
{
	WRITE_ONCE(*ptep, new_pte);
	__dsb(ishst);
}

static inline void stage1_set_pmd(pmd_t *pmdp, pmd_t new_pmd)
{
	WRITE_ONCE(*pmdp, new_pmd);
	__dsb(ishst);
}

static inline void stage1_set_pud(pud_t *pudp, pud_t new_pud)
{
	WRITE_ONCE(*pudp, new_pud);
	__dsb(ishst);
}

static inline void stage1_set_pgd(pgd_t *pgdp, pgd_t new_pgd)
{
	WRITE_ONCE(*pgdp, new_pgd);
	__dsb(ishst);
}

static inline void stage1_pgd_populate(pgd_t *pgdp, unsigned long addr, unsigned long flags)
{
	stage1_set_pgd(pgdp, vtop(addr) | S1_DES_TABLE);
}

static inline void stage1_pud_populate(pud_t *pudp, unsigned long addr, unsigned long flags)
{
	stage1_set_pud(pudp, vtop(addr) | S1_DES_TABLE);
}

static inline void stage1_pmd_populate(pmd_t *pmdp, unsigned long addr, unsigned long flags)
{
	stage1_set_pmd(pmdp, vtop(addr) | S1_DES_TABLE);
}

static inline pmd_t stage1_pmd_attr(unsigned long phy, unsigned long flags)
{
	pmd_t pmd = phy & S1_PMD_MASK;

	switch (flags & VM_TYPE_MASK) {
	case __VM_NORMAL_NC:
		pmd |= S1_BLOCK_NORMAL_NC;
		break;
	case __VM_IO:
		pmd |= S1_BLOCK_DEVICE;
		break;
	case __VM_WT:
		pmd |= S1_BLOCK_WT;
		break;
	default:
		pmd |= S1_BLOCK_NORMAL;
		break;
	}

	if ((flags & VM_RW_MASK) == __VM_RO)
		pmd |= S1_AP_RO;
	else
		pmd |= S1_AP_RW;

	if (!(flags & __VM_EXEC))
		pmd |= (S1_XN | S1_PXN);

	if (flags & __VM_PFNMAP)
		pmd |= S1_PFNMAP;

	if (flags & __VM_DEVMAP)
		pmd |= S1_DEVMAP;

	if (flags & __VM_SHARED)
		pmd |= S1_SHARED;

	return pmd;
}

static inline pte_t stage1_pte_attr(unsigned long phy, unsigned long flags)
{
	pte_t pte = phy & S1_PTE_MASK;

	switch (flags & VM_TYPE_MASK) {
	case __VM_NORMAL_NC:
		pte |= S1_PAGE_NORMAL_NC;
		break;
	case __VM_IO:
		pte |= S1_PAGE_DEVICE;
		break;
	case __VM_WT:
		pte |= S1_PAGE_WT;
		break;
	default:
		pte |= S1_PAGE_NORMAL;
		break;
	}

	if ((flags & VM_RW_MASK) == __VM_RO)
		pte |= S1_AP_RO;
	else
		pte |= S1_AP_RW;

	if (!(flags & __VM_EXEC))
		pte |= (S1_XN | S1_PXN);

	if (flags & __VM_PFNMAP)
		pte |= S1_PFNMAP;

	if (flags & __VM_DEVMAP)
		pte |= S1_DEVMAP;

	if (flags & __VM_SHARED)
		pte |= S1_SHARED;

	return pte;
}

static void stage1_unmap_pte_range(struct vspace *vs, pte_t *ptep,
		unsigned long addr, unsigned long end)
{
	pte_t *pte;

	pte = stage1_pte_offset(ptep, addr);

	do {
		if (!stage1_pte_none(*pte))
			stage1_set_pte(pte, 0);
	} while (pte++, addr += PAGE_SIZE, addr != end);
}

static void stage1_unmap_pmd_range(struct vspace *vs, pmd_t *pmdp,
		unsigned long addr, unsigned long end)
{
	unsigned long next;
	pmd_t *pmd;
	pte_t *ptep;

	pmd = stage1_pmd_offset(pmdp, addr);

	do {
		next = stage1_pmd_addr_end(addr, end);
		if (!stage1_pmd_none(*pmd)) {
			if (stage1_pmd_huge(*pmd)) {
				stage1_pmd_clear(pmd);
			} else {
				ptep = (pte_t *)ptov(stage1_pte_table_addr(*pmd));
				stage1_unmap_pte_range(vs, ptep, addr, next);
			}
		}
	} while (pmd++, addr = next, addr != end);
}

static int stage1_unmap_pud_range(struct vspace *vs, unsigned long addr, unsigned long end)
{
	unsigned long next;
	pud_t *pud;
	pmd_t *pmdp;

	pud = stage1_pud_offset((pud_t *)vs->pgdp, end);
	do {
		next = stage1_pud_addr_end(addr, end);
		if (!stage1_pud_none(*pud)) {
			pmdp = (pmd_t *)ptov(stage1_pmd_table_addr(*pud));
			stage1_unmap_pmd_range(vs, pmdp, addr, next);
		}
	} while (pud++, addr = next, addr != end);

	flush_all_tlb_host();

	return 0;
}

static int stage1_map_pte_range(struct vspace *vs, pte_t *ptep, unsigned long start,
		unsigned long end, unsigned long physical, unsigned long flags)
{
	unsigned long pte_attr;
	pte_t *pte;
	pte_t old_pte;

	pte = stage1_pte_offset(ptep, start);
	pte_attr = stage1_pte_attr(0, flags);

	do {
		old_pte = *pte;
		if (old_pte)
			pr_err("error: pte remaped 0x%x\n", start);
		stage1_set_pte(pte, pte_attr | physical);
	} while (pte++, start += PAGE_SIZE, physical += PAGE_SIZE, start != end);

	return 0;
}

static inline bool stage1_pmd_huge_page(pmd_t old_pmd, unsigned long start,
		unsigned long phy, size_t size, unsigned long flags)
{
	if (!(flags & __VM_HUGE_2M) || old_pmd)
		return false;

	if (!IS_BLOCK_ALIGN(start) || !IS_BLOCK_ALIGN(phy) || !(IS_BLOCK_ALIGN(size)))
		return false;

	return true;
}

static int stage1_map_pmd_range(struct vspace *vs, pmd_t *pmdp, unsigned long start,
		unsigned long end, unsigned long physical, unsigned long flags)
{
	unsigned long next;
	unsigned long attr;
	pmd_t *pmd;
	pmd_t old_pmd;
	pte_t *ptep;
	size_t size;
	int ret;

	pmd = stage1_pmd_offset(pmdp, start);
	do {
		next = stage1_pmd_addr_end(start, end);
		size = next - start;
		old_pmd = *pmd;

		/*
		 * virtual memory need to map as PMD huge page
		 */
		if (stage1_pmd_huge_page(old_pmd, start, physical, size, flags)) {
			attr = stage1_pmd_attr(physical, flags);
			stage1_set_pmd(pmd, attr);
		} else {
			if (stage1_pmd_none(old_pmd)) {
				ptep = (pte_t *)stage1_get_free_page(flags);
				if (!ptep)
					return -ENOMEM;
				memset(ptep, 0, PAGE_SIZE);
				stage1_pmd_populate(pmd, (unsigned long)ptep, flags);
			} else {
				ptep = (pte_t *)ptov(stage1_pte_table_addr(old_pmd));
			}

			ret = stage1_map_pte_range(vs, ptep, start, next, physical, flags);
			if (ret)
				return ret;
		}
	} while (pmd++, physical += size, start = next, start != end);

	return 0;
}

static int stage1_map_pud_range(struct vspace *vs, unsigned long start,
		unsigned long end, unsigned long physical, unsigned long flags)
{
	unsigned long next;
	pud_t *pud;
	pmd_t *pmdp;
	size_t size;
	int ret;

	pud = stage1_pud_offset((pud_t *)vs->pgdp, start);
	do {
		next = stage1_pud_addr_end(start, end);
		size = next - start;

		if (stage1_pud_none(*pud)) {
			pmdp = (pmd_t *)stage1_get_free_page(flags);
			if (!pmdp)
				return -ENOMEM;
			memset(pmdp, 0, PAGE_SIZE);
			stage1_pud_populate(pud, (unsigned long)pmdp, flags);
		} else {
			pmdp = (pmd_t *)ptov(stage1_pmd_table_addr(*pud));
		}

		ret = stage1_map_pmd_range(vs, pmdp, start, next, physical, flags);
		if (ret)
			return ret;
	} while (pud++, physical += size, start = next, start != end);

	return 0;
}

static int stage1_get_leaf_entry(struct vspace *vs,
		unsigned long va, pmd_t **pmdpp, pte_t **ptepp)
{
	pud_t *pudp;
	pmd_t *pmdp;
	pte_t *ptep;

	pudp = stage1_pud_offset(vs->pgdp, va);
	if (stage1_pud_none(*pudp))
		return -ENOMEM;

	pmdp = stage1_pmd_offset(stage1_pmd_table_addr(*pudp), va);
	if (stage1_pmd_none(*pmdp))
		return -ENOMEM;

	if (stage1_pmd_huge(*pmdp)) {
		*pmdpp = pmdp;
		return 0;
	}

	ptep = stage1_pte_offset(stage1_pte_table_addr(*pmdp), va);
	*ptepp = ptep;

	return 0;
}

int arch_host_change_map(struct vspace *vs, unsigned long vir,
		unsigned long phy, unsigned long flags)
{
	int ret;
	pmd_t *pmdp = NULL;
	pte_t *ptep = NULL;

	ret = stage1_get_leaf_entry(vs, vir, &pmdp, &ptep);
	if (ret)
		return ret;

	if (pmdp && !ptep) {
		stage1_set_pmd(pmdp, 0);
		flush_tlb_va_range(vir, S1_PMD_SIZE);
		stage1_set_pmd(pmdp, stage1_pmd_attr(phy, flags));
		return 0;
	}

	stage1_set_pte(ptep, 0);
	flush_tlb_va_range(vir, S1_PTE_SIZE);
	stage1_set_pte(ptep, stage1_pte_attr(phy, flags));

	return 0;
}

static inline phy_addr_t stage1_va_to_pa(struct vspace *vs, unsigned long va)
{
	unsigned long pte_offset = va & ~S1_PTE_MASK;
	unsigned long pmd_offset = va & ~S1_PMD_MASK;
	unsigned long phy = 0;
	pud_t *pudp;
	pmd_t *pmdp;
	pte_t *ptep;

	pudp = stage1_pud_offset(vs->pgdp, va);
	if (stage1_pud_none(*pudp))
		return 0;

	pmdp = stage1_pmd_offset(ptov(stage1_pmd_table_addr(*pudp)), va);
	if (stage1_pmd_none(*pmdp))
		return 0;

	if (stage1_pmd_huge(*pmdp)) {
		phy = ((*pmdp) & S1_PHYSICAL_MASK) + pmd_offset;
		return 0;
	}

	ptep = stage1_pte_offset(ptov(stage1_pte_table_addr(*pmdp)), va);
	phy = *ptep & S1_PHYSICAL_MASK;
	if (phy == 0)
		return 0;

	return phy + pte_offset;
}

phy_addr_t arch_translate_va_to_pa(struct vspace *vs, unsigned long va)
{
	return stage1_va_to_pa(vs, va);
}

int arch_host_map(struct vspace *vs, unsigned long start, unsigned long end,
		unsigned long physical, unsigned long flags)
{
	if (end == start)
		return -EINVAL;

	ASSERT((start < S1_VIRT_MAX) && (end <= S1_VIRT_MAX));
	ASSERT(physical < S1_PHYSICAL_MAX);
	ASSERT(IS_PAGE_ALIGN(start) && IS_PAGE_ALIGN(end) && IS_PAGE_ALIGN(physical));

	return stage1_map_pud_range(vs, start, end, physical, flags);
}

int arch_host_unmap(struct vspace *vs, unsigned long start, unsigned long end)
{
	ASSERT((start < S1_VIRT_MAX) && (end <= S1_VIRT_MAX));
	return stage1_unmap_pud_range(vs, start, end);
}

unsigned long arch_kernel_pgd_base(void)
{
	extern unsigned char __stage1_page_table;

	return (unsigned long)&__stage1_page_table;
}
