#ifndef __MINOS_ARCH_PAGETABLE_H__
#define __MINOS_ARCH_PAGETABLE_H__

#include <minos/types.h>
#include <minos/memattr.h>
#include <asm/asm_mmu.h>

#define __PGD_RANGE_OFFSET		(39)
#define __PGD_DES_OFFSET		(12)
#define __PGD_ENTRY_OFFSET_MASK		(0x1ffUL << __PGD_RANGE_OFFSET)

#define __PUD_RANGE_OFFSET		(30)
#define __PUD_DES_OFFSET		(12)
#define __PUD_ENTRY_OFFSET_MASK		(0x1ffUL << __PUD_RANGE_OFFSET)

#define __PMD_RANGE_OFFSET		(21)
#define __PMD_DES_OFFSET		(12)
#define __PMD_ENTRY_OFFSET_MASK		(0x1ffUL << __PMD_RANGE_OFFSET)

#define __PTE_RANGE_OFFSET		(12)
#define __PTE_DES_OFFSET		(12)
#define __PTE_ENTRY_OFFSET_MASK		(0x1ffUL << __PTE_RANGE_OFFSET)

#define __GVM_PGD_PAGE_NR		(2)
#define __GVM_PGD_PAGE_ALIGN		(2)

#define __PAGETABLE_ATTR_MASK		(0x0000ffffffe00000UL)

typedef unsigned long __pgd_t;
typedef unsigned long __pud_t;
typedef unsigned long __pmd_t;
typedef unsigned long __pte_t;

unsigned long arch_page_table_description(unsigned long flags);
int get_mapping_type(int lvl, unsigned long addr);

#endif
