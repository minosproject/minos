#ifndef _MINOS_MMU_H_
#define _MINOS_MMU_H_

#include <minos/types.h>
#include <config/config.h>
#include <minos/memattr.h>
#include <asm/pagetable.h>

typedef __pgd_t pgd_t;
typedef __pud_t pud_t;
typedef __pmd_t pmd_t;
typedef __pte_t pte_t;

#define PGD_RANGE_OFFSET	(__PGD_RANGE_OFFSET)
#define PGD_DES_OFFSET		(__PGD_DES_OFFSET)
#define PGD_ENTRY_OFFSET_MASK	(__PGD_ENTRY_OFFSET_MASK)

#define PUD_RANGE_OFFSET	(__PUD_RANGE_OFFSET)
#define PUD_DES_OFFSET		(__PUD_DES_OFFSET)
#define PUD_ENTRY_OFFSET_MASK	(__PUD_ENTRY_OFFSET_MASK)

#define PMD_RANGE_OFFSET	(__PMD_RANGE_OFFSET)
#define PMD_DES_OFFSET		(__PMD_DES_OFFSET)
#define PMD_ENTRY_OFFSET_MASK	(__PMD_ENTRY_OFFSET_MASK)

#define PTE_RANGE_OFFSET	(__PTE_RANGE_OFFSET)
#define PTE_DES_OFFSET		(__PTE_DES_OFFSET)
#define PTE_ENTRY_OFFSET_MASK	(__PTE_ENTRY_OFFSET_MASK)

#define PAGETABLE_ATTR_MASK 	(__PAGETABLE_ATTR_MASK)

#define PGD_MAP_SIZE		(1UL << PGD_RANGE_OFFSET)
#define PUD_MAP_SIZE		(1UL << PUD_RANGE_OFFSET)
#define PMD_MAP_SIZE		(1UL << PMD_RANGE_OFFSET)
#define PTE_MAP_SIZE		(1UL << PTE_RANGE_OFFSET)

#define PAGE_MAPPING_COUNT	(PAGE_SIZE / sizeof(pgd_t))

#define PGD_SHIFT		PGD_RANGE_OFFSET
#define PUD_SHIFT		PUD_RANGE_OFFSET
#define PMD_SHIFT		PMD_RANGE_OFFSET
#define PTE_SHIFT		PMD_RANGE_OFFSET

#define PGD_MASK		(~(PGD_MAP_SIZE - 1))
#define PUD_MASK		(~(PUD_MAP_SIZE - 1))
#define PMD_MASK		(~(PMD_MAP_SIZE - 1))
#define PTE_MASK		(~(PTE_MAP_SIZE - 1))

#define PGD_NOT_MAPPED		(PGD + 1)
#define PUD_NOT_MAPPED		(PUD + 1)
#define PMD_NOT_MAPPED		(PMD + 1)
#define PTE_NOT_MAPPED		(PTE + 1)
#define INVALID_MAPPING		(6)

/* for early mapping use */
#define VM_DESC_HOST_TABLE	__VM_DESC_HOST_TABLE
#define VM_DESC_HOST_BLOCK	__VM_DESC_HOST_BLOCK

#define mapping_error(r)	(((unsigned long)(r) > 0) && ((unsigned long)(r) <= 6))

#define pgd_idx(vir)		((vir >> PGD_SHIFT) & (PAGE_MAPPING_COUNT - 1))
#define pud_idx(vir)		((vir >> PUD_SHIFT) & (PAGE_MAPPING_COUNT - 1))
#define pmd_idx(vir)		((vir >> PMD_SHIFT) & (PAGE_MAPPING_COUNT - 1))
#define ptd_idx(vir)		((vir >> PTE_SHIFT) & (PAGE_MAPPING_COUNT - 1))

#ifdef ARCH_AARCH64
#define guest_pgd_idx(vir)	BUG()
#define guest_pud_idx(vir)	((vir >> PUD_SHIFT) & ((PAGE_MAPPING_COUNT * 2) - 1))
#define guest_pmd_idx(vir)	pmd_idx(vir)
#define guest_pte_idx(vir)	pte_idx(vir)
#else
#define guest_pgd_idx(vir)	pgd_idx(vir)
#define guest_pud_idx(vir)	pud_idx(vir)
#define guest_pmd_idx(vir)	pmd_idx(vir)
#define guest_pte_idx(vir)	pte_idx(vir)
#endif

#define pgd_offset(ppgd, vir)	((pgd_t *)ppgd + pgd_idx(vir))
#define pud_offset(ppud, vir)	((pud_t *)ppud + pud_idx(vir))
#define pmd_offset(ppmd, vir)	((pmd_t *)ppmd + pmd_idx(vir))
#define pte_offset(ppte, vir)	((pte_t *)ppte + pte_idx(vir))

#ifdef ARCH_AARCH64
#define guest_pgd_offset(ppgd, vir) 	((pgd_t *)ppgd)
#else
#define guest_pgd_offset(ppgd, vir) 	pgd_offset(ppgd, vir)
#endif

#define guest_pud_offset(ppud, vir) 	((pud_t *)ppud + guest_pud_idx(vir))
#define guest_pmd_offset(ppmd, vir)	pmd_offset(ppmu, vir)
#define guest_pte_offset(ppte, vir)	pte_offset(ppte, vir)

#define get_pmd(ppud) (pmd_t *)((*(pud_t *)ppud) & PAGETABLE_ATTR_MASK)

#define set_pte_at(ptep, val)	(*(pte_t *)ptep = val)
#define set_pmd_at(pmdp, val)	(*(pmd_t *)pmdp = val)
#define set_pud_at(pudp, val)	(*(pud_t *)pudp = val)
#define set_pgd_at(pgdp, val)	(*(pgd_t *)pgdp = val)

struct mm_struct;

struct mapping_struct {
	unsigned long table_base;
	unsigned long vir_base;
	unsigned long phy_base;
	size_t size;
	int lvl;
	unsigned long flags;
	struct pagetable_attr *config;
};

int create_mem_mapping(struct mm_struct *mm, unsigned long addr,
		unsigned long phy, size_t size, unsigned long flags);

int destroy_mem_mapping(struct mm_struct *mm, unsigned long vir,
		size_t size, unsigned long flags);

unsigned long get_mapping_entry(unsigned long tt,
		unsigned long vir, int start, int end);

unsigned long page_table_description(unsigned long flags);

static inline unsigned long
get_mapping_pte(unsigned long pgd, unsigned long vir, unsigned long flags)
{
	if (flags & VM_HOST)
		return get_mapping_entry(pgd, vir, PGD, PTE);
	else
		return get_mapping_entry(pgd, vir, PUD, PTE);
}

static inline unsigned long
get_mapping_pud(unsigned long pgd, unsigned long vir, unsigned long flags)
{
	if (flags & VM_HOST)
		return get_mapping_entry(pgd, vir, PGD, PUD);
	else
		return 0;
}

static inline unsigned long
get_mapping_pmd(unsigned long pgd, unsigned long vir, unsigned long flags)
{
	if (flags & VM_HOST)
		return get_mapping_entry(pgd, vir, PGD, PMD);
	else
		return get_mapping_entry(pgd, vir, PUD, PMD);
}

void create_pud_mapping(unsigned long pud, unsigned long vir,
		unsigned long value, unsigned long flags);
void create_pmd_mapping(unsigned long pmd, unsigned long vir,
		unsigned long value, unsigned long flags);
void create_pte_mapping(unsigned long pte, unsigned long vir,
		unsigned long value, unsigned long flags);
void create_pgd_mapping(unsigned long pgd, unsigned long vir,
		unsigned long value, unsigned long flags);

unsigned long alloc_guest_pmd(struct mm_struct *mm, unsigned long phy);

#endif
