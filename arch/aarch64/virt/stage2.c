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
#include <virt/vmm.h>
#include <asm/tlb.h>
#include <asm/cache.h>
#include "stage2.h"

void *arch_alloc_guest_pgd(void)
{
	void *page;

	page = __get_free_pages(2, 2);
	if (page)
		memset(page, 0, PAGE_SIZE * 2);

	return page;
}

static void inline flush_tlb_va_range(unsigned long va, size_t size)
{
	flush_tlb_ipa_guest(va, size);
}

static void inline stage2_pgd_clear(pud_t *pgdp)
{
	WRITE_ONCE(*pgdp, 0);
	__dsb(ishst);
	isb();
}

static void inline stage2_pud_clear(pud_t *pudp)
{
	WRITE_ONCE(*pudp, 0);
	__dsb(ishst);
	isb();
}

static void inline stage2_pmd_clear(pmd_t *pmdp)
{
	WRITE_ONCE(*pmdp, 0);
	__dsb(ishst);
	isb();
}

static void *stage2_get_free_page(unsigned long flags)
{
	return get_free_page();
}

static unsigned long stage2_xxx_addr_end(unsigned long start, unsigned long end, size_t map_size)
{
	unsigned long boundary = (start + map_size) & ~((unsigned long)map_size - 1);

	return ((boundary - 1) < (end - 1)) ? boundary : end;
}

#define stage2_pgd_addr_end(start, end)	\
	stage2_xxx_addr_end(start, end, S2_PGD_SIZE)

#define stage2_pud_addr_end(start, end)	\
	stage2_xxx_addr_end(start, end, S2_PUD_SIZE)

#define stage2_pmd_addr_end(start, end)	\
	stage2_xxx_addr_end(start, end, S2_PMD_SIZE)

static inline void stage2_set_pte(pte_t *ptep, pte_t new_pte)
{
	WRITE_ONCE(*ptep, new_pte);
	__dsb(ishst);
}

static inline void stage2_set_pmd(pmd_t *pmdp, pmd_t new_pmd)
{
	WRITE_ONCE(*pmdp, new_pmd);
	__dsb(ishst);
}

static inline void stage2_set_pud(pud_t *pudp, pud_t new_pud)
{
	WRITE_ONCE(*pudp, new_pud);
	__dsb(ishst);
}

static inline void stage2_set_pgd(pgd_t *pgdp, pgd_t new_pgd)
{
	WRITE_ONCE(*pgdp, new_pgd);
	__dsb(ishst);
}

static inline void stage2_pgd_populate(pgd_t *pgdp, unsigned long addr, unsigned long flags)
{
	uint64_t attrs = S2_DES_TABLE;

	stage2_set_pgd(pgdp, vtop(addr) | attrs);
}

static inline void stage2_pud_populate(pud_t *pudp, unsigned long addr, unsigned long flags)
{
	uint64_t attrs = S2_DES_TABLE;

	stage2_set_pud(pudp, vtop(addr) | attrs);
}

static inline void stage2_pmd_populate(pmd_t *pmdp, unsigned long addr, unsigned long flags)
{
	uint64_t attrs = S2_DES_TABLE;

	stage2_set_pmd(pmdp, vtop(addr) | attrs);
}

static inline pmd_t stage2_pmd_attr(unsigned long phy, unsigned long flags)
{
	pmd_t pmd = phy & S2_PMD_MASK;

	switch (flags & VM_TYPE_MASK) {
	case __VM_NORMAL_NC:
		pmd |= S2_BLOCK_NORMAL_NC;
		break;
	case __VM_IO:
		pmd |= S2_BLOCK_DEVICE;
		break;
	case __VM_WT:
		pmd |= S2_BLOCK_WT;
		break;
	default:
		pmd |= S2_BLOCK_NORMAL;
		break;
	}

	switch (flags & VM_RW_MASK) {
	case __VM_RO:
		pmd |= S2_AP_RO;
		break;
	case __VM_RW:
		pmd |= S2_AP_RW;
		break;
	case __VM_WO:
		pmd |= S2_AP_WO;
		break;
	default:
		pmd |= S2_AP_NON;
		break;
	}

	if (!(flags & __VM_EXEC))
		pmd |= S2_XN;

	if (flags & __VM_PFNMAP)
		pmd |= S2_PFNMAP;

	if (flags & __VM_DEVMAP)
		pmd |= S2_DEVMAP;

	if (flags & __VM_SHARED)
		pmd |= S2_SHARED;

	return pmd;
}

static inline pte_t stage2_pte_attr(unsigned long phy, unsigned long flags)
{
	pte_t pte = phy & S2_PTE_MASK;

	switch (flags & VM_TYPE_MASK) {
	case __VM_NORMAL_NC:
		pte |= S2_PAGE_NORMAL_NC;
		break;
	case __VM_IO:
		pte |= S2_PAGE_DEVICE;
		break;
	case __VM_WT:
		pte |= S2_PAGE_WT;
		break;
	default:
		pte |= S2_PAGE_NORMAL;
		break;
	}

	switch (flags & VM_RW_MASK) {
	case __VM_RO:
		pte |= S2_AP_RO;
		break;
	case __VM_RW:
		pte |= S2_AP_RW;
		break;
	case __VM_WO:
		pte |= S2_AP_WO;
		break;
	default:
		pte |= S2_AP_NON;
		break;
	}

	if (!(flags & __VM_EXEC))
		pte |= S2_XN;

	if (flags & __VM_PFNMAP)
		pte |= S2_PFNMAP;

	if (flags & __VM_DEVMAP)
		pte |= S2_DEVMAP;

	if (flags & __VM_SHARED)
		pte |= S2_SHARED;

	return pte;
}

static void stage2_unmap_pte_range(struct mm_struct *vs, pte_t *ptep,
		unsigned long addr, unsigned long end)
{
	pte_t *pte;

	pte = stage2_pte_offset(ptep, addr);

	do {
		if (!stage2_pte_none(*pte))
			stage2_set_pte(pte, 0);
	} while (pte++, addr += PAGE_SIZE, addr != end);
}

static inline bool is_pmd_range(unsigned long start, unsigned long end)
{
        if (((start & (S2_PMD_SIZE - 1)) == 0) && ((end - start) == S2_PMD_SIZE))
                return true;
        return false;
}

static inline bool is_pud_range(unsigned long start, unsigned long end)
{
        if (((start & (S2_PUD_SIZE - 1)) == 0) && ((end - start) == S2_PUD_SIZE))
                return true;
        return false;
}

static void stage2_unmap_pmd_range(struct mm_struct *vs, pmd_t *pmdp,
		unsigned long addr, unsigned long end)
{
	unsigned long next;
	pmd_t *pmd;
	pte_t *ptep;

	pmd = stage2_pmd_offset(pmdp, addr);

	do {
		next = stage2_pmd_addr_end(addr, end);
		if (!stage2_pmd_none(*pmd)) {
			if (stage2_pmd_huge(*pmd)) {
				stage2_pmd_clear(pmd);
			} else {
				ptep = (pte_t *)ptov(stage2_pte_table_addr(*pmd));
				stage2_unmap_pte_range(vs, ptep, addr, next);
				if (is_pmd_range(addr, next)) {
					stage2_pmd_clear(pmd);
					free_pages(ptep);
				}
			}
		}
	} while (pmd++, addr = next, addr != end);
}

static int stage2_unmap_pud_range(struct mm_struct *vs, unsigned long addr, unsigned long end)
{
	unsigned long next;
	pud_t *pud;
	pmd_t *pmdp;

	pud = stage2_pud_offset((pud_t *)vs->pgdp, end);
	do {
		next = stage2_pud_addr_end(addr, end);
		if (!stage2_pud_none(*pud)) {
			pmdp = (pmd_t *)ptov(stage2_pmd_table_addr(*pud));
			stage2_unmap_pmd_range(vs, pmdp, addr, next);
			if (is_pud_range(addr, next)) {
				stage2_pud_clear(pud);
				free_pages(pmdp);
			}
		}
	} while (pud++, addr = next, addr != end);

	flush_all_tlb_guest();

	return 0;
}

static int stage2_map_pte_range(struct mm_struct *vs, pte_t *ptep, unsigned long start,
		unsigned long end, unsigned long physical, unsigned long flags)
{
	unsigned long pte_attr;
	pte_t *pte;
	pte_t old_pte;

	pte = stage2_pte_offset(ptep, start);
	pte_attr = stage2_pte_attr(0, flags);

	do {
		old_pte = *pte;
		if (old_pte)
			pr_err("error: pte remaped 0x%x\n", start);
		stage2_set_pte(pte, pte_attr | physical);
	} while (pte++, start += PAGE_SIZE, physical += PAGE_SIZE, start != end);

	return 0;
}

static inline bool stage2_pmd_huge_page(pmd_t old_pmd, unsigned long start,
		unsigned long phy, size_t size, unsigned long flags)
{
	if (!(flags & __VM_HUGE_2M) || old_pmd)
		return false;

	if (!IS_BLOCK_ALIGN(start) || !IS_BLOCK_ALIGN(phy) || !(IS_BLOCK_ALIGN(size)))
		return false;

	return true;
}

static int stage2_map_pmd_range(struct mm_struct *vs, pmd_t *pmdp, unsigned long start,
		unsigned long end, unsigned long physical, unsigned long flags)
{
	unsigned long next;
	unsigned long attr;
	pmd_t *pmd;
	pmd_t old_pmd;
	pte_t *ptep;
	size_t size;
	int ret;

	pmd = stage2_pmd_offset(pmdp, start);
	do {
		next = stage2_pmd_addr_end(start, end);
		size = next - start;
		old_pmd = *pmd;

		/*
		 * virtual memory need to map as PMD huge page
		 */
		if (stage2_pmd_huge_page(old_pmd, start, physical, size, flags)) {
			attr = stage2_pmd_attr(physical, flags);
			stage2_set_pmd(pmd, attr);
		} else {
			if (old_pmd && ((old_pmd & 0x03) == S2_DES_BLOCK)) {
				pr_err("stage2: vaddr 0x%x has mapped as huge page\n", start);
				return -EINVAL;
			}

			if (stage2_pmd_none(old_pmd)) {
				ptep = (pte_t *)stage2_get_free_page(flags);
				if (!ptep)
					return -ENOMEM;
				memset(ptep, 0, PAGE_SIZE);
				stage2_pmd_populate(pmd, (unsigned long)ptep, flags);
			} else {
				ptep = (pte_t *)ptov(stage2_pte_table_addr(old_pmd));
			}

			ret = stage2_map_pte_range(vs, ptep, start, next, physical, flags);
			if (ret)
				return ret;
		}
	} while (pmd++, physical += size, start = next, start != end);

	return 0;
}

static int stage2_map_pud_range(struct mm_struct *vs, unsigned long start,
		unsigned long end, unsigned long physical, unsigned long flags)
{
	unsigned long next;
	pud_t *pud;
	pmd_t *pmdp;
	size_t size;
	int ret;

	pud = stage2_pud_offset((pud_t *)vs->pgdp, start);
	do {
		next = stage2_pud_addr_end(start, end);
		size = next - start;

		if (stage2_pud_none(*pud)) {
			pmdp = (pmd_t *)stage2_get_free_page(flags);
			if (!pmdp)
				return -ENOMEM;
			memset(pmdp, 0, PAGE_SIZE);
			stage2_pud_populate(pud, (unsigned long)pmdp, flags);
		} else {
			pmdp = (pmd_t *)ptov(stage2_pmd_table_addr(*pud));
		}

		ret = stage2_map_pmd_range(vs, pmdp, start, next, physical, flags);
		if (ret)
			return ret;
	} while (pud++, physical += size, start = next, start != end);

	return 0;
}

static inline int stage2_ipa_to_pa(struct mm_struct *vs,
		unsigned long va, phy_addr_t *pa)
{
	unsigned long pte_offset = va & ~S2_PTE_MASK;
	unsigned long pmd_offset = va & ~S2_PMD_MASK;
	unsigned long phy = 0;
	pud_t *pudp;
	pmd_t *pmdp;
	pte_t *ptep;

	pudp = stage2_pud_offset(vs->pgdp, va);
	if (stage2_pud_none(*pudp))
		return -EFAULT;

	pmdp = stage2_pmd_offset(ptov(stage2_pmd_table_addr(*pudp)), va);
	if (stage2_pmd_none(*pmdp))
		return -EFAULT;

	if (stage2_pmd_huge(*pmdp)) {
		*pa = ((*pmdp) & S2_PHYSICAL_MASK) + pmd_offset;
		return 0;
	}

	ptep = stage2_pte_offset(ptov(stage2_pte_table_addr(*pmdp)), va);
	phy = *ptep & S2_PHYSICAL_MASK;
	if (phy == 0)
		return -EFAULT;

	*pa = phy + pte_offset;

	return 0;
}

int arch_translate_guest_ipa(struct mm_struct *vs,
		unsigned long va, phy_addr_t *pa)
{
	return stage2_ipa_to_pa(vs, va, pa);
}

int arch_guest_map(struct mm_struct *vs, unsigned long start, unsigned long end,
		unsigned long physical, unsigned long flags)
{
	if (end == start)
		return -EINVAL;

	ASSERT((start < VMM_VIRT_MAX) && (end <= VMM_VIRT_MAX));
	ASSERT(physical < S2_PHYSICAL_MAX);
	ASSERT(IS_PAGE_ALIGN(start) && IS_PAGE_ALIGN(end) && IS_PAGE_ALIGN(physical));

	return stage2_map_pud_range(vs, start, end, physical, flags);
}

int arch_guest_unmap(struct mm_struct *vs, unsigned long start, unsigned long end)
{
	if (end == start)
		return -EINVAL;

	ASSERT((start < VMM_VIRT_MAX) && (end <= VMM_VIRT_MAX));
	return stage2_unmap_pud_range(vs, start, end);
}
