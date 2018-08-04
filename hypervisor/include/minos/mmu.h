#ifndef _MINOS_MMU_H_
#define _MINOS_MMU_H_

#include <minos/types.h>
#include <minos/memattr.h>
#include <asm/pagetable.h>

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

#define PAGE_MAPPING_COUNT	(PAGE_SIZE / sizeof(unsigned long))

#define PGD_NOT_MAPPED		(PGD + 1)
#define PUD_NOT_MAPPED		(PUD + 1)
#define PMD_NOT_MAPPED		(PMD + 1)
#define PTE_NOT_MAPPED		(PTE + 1)
#define INVALID_MAPPING		(6)

#define mapping_error(r)	(((unsigned long)(r) > 0) && ((unsigned long)(r) <= 6))

#define pud_offset(vir)		((vir - ALIGN(vir, PGD_OFFSET)) >> PUD_RANGE_OFFSET)
#define pmd_offset(vir)		((vir - ALIGN(vir, PUD_MAP_SIZE)) >> PMD_RANGE_OFFSET)
#define pte_offset(vir)		((vir - ALIGN(vir, PMD_MAP_SIZE)) >> PTE_RANGE_OFFSET)

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
get_mapping_pgd(unsigned long pgd, unsigned long vir, unsigned long flags)
{
	if (!(flags & VM_HOST))
		return 0;

	return get_mapping_entry(pgd, vir, PGD, PGD);
}

static inline unsigned long
get_mapping_pud(unsigned long pgd, unsigned long vir, unsigned long flags)
{
	if (flags & VM_HOST)
		return get_mapping_entry(pgd, vir, PGD, PUD);
	else
		return get_mapping_entry(pgd, vir, PUD, PUD);
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

unsigned long alloc_guest_pud(struct mm_struct *mm, unsigned long phy);

#endif
