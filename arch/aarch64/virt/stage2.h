#ifndef __MINOS_ARM64_STAGE2_H__
#define __MINOS_ARM64_STAGE2_H__

#include <minos/types.h>

/*
 * Stage 2 VMSAv8-64 Table Descriptors
 */
#define S2_DES_FAULT			(0b00 << 0)
#define S2_DES_BLOCK			(0b01 << 0)
#define S2_DES_TABLE			(0b11 << 0)
#define S2_DES_PAGE			(0b11 << 0)

/*
 * Stage 2 VMSAv8-64 Page / Block Descriptors
 */
#define S2_CONTIGUOUS			(1UL << 52)
#define S2_XN				(1UL << 54)
#define S2_AF				(1UL << 10)

#define S2_PFNMAP			(1UL << 55)	// bit 55 - 58 is for software use
#define S2_DEVMAP			(1UL << 56)	// bit 55 - 58 is for software use
#define S2_SHARED			(1UL << 57)	// bit 55 - 58 is for software use

#define S2_SH_NON			(0b00 << 8)
#define S2_SH_OUTER			(0b10 << 8)
#define S2_SH_INNER			(0b11 << 8)

#define S2_AP_NON			(0b00 << 6)
#define S2_AP_RO			(0b01 << 6)
#define S2_AP_WO			(0b10 << 6)
#define S2_AP_RW			(0b11 << 6)

#define S2_MEMATTR_DEV_nGnRnE		(0b0000 << 2)
#define S2_MEMATTR_DEV_nGnRE		(0b0001 << 2)
#define S2_MEMATTR_DEV_nGRE		(0b0010 << 2)
#define S2_MEMATTR_DEV_GRE		(0b0011 << 2)

#define S2_MEMATTR_NORMAL_WB		(0b1111 << 2)
#define S2_MEMATTR_NORMAL_NC		(0b0101 << 2)
#define S2_MEMATTR_NORMAL_WT		(0b1010 << 2)

#define S2_MT_NORMAL			0
#define S2_MT_DEVICE			1

#define S2_CACHE_WB			0
#define S2_CACHE_NC			1

#define S2_PAGE_NORMAL			(S2_DES_PAGE | S2_AF | S2_SH_INNER | S2_MEMATTR_NORMAL_WB)
#define S2_PAGE_NORMAL_NC		(S2_DES_PAGE | S2_AF | S2_SH_INNER | S2_MEMATTR_NORMAL_NC)
#define S2_PAGE_WT			(S2_DES_PAGE | S2_AF | S2_SH_INNER | S2_MEMATTR_NORMAL_WT)
// #define S2_PAGE_DEVICE		(S2_DES_PAGE | S2_AF | S2_SH_OUTER | S2_MEMATTR_DEV_nGnRnE | S2_XN)
#define S2_PAGE_DEVICE			(S2_DES_PAGE | S2_AF | S2_SH_OUTER | S2_MEMATTR_DEV_nGnRE | S2_XN)

#define S2_BLOCK_NORMAL			(S2_DES_BLOCK | S2_AF | S2_SH_INNER | S2_MEMATTR_NORMAL_WB)
#define S2_BLOCK_NORMAL_NC		(S2_DES_BLOCK | S2_AF | S2_SH_INNER | S2_MEMATTR_NORMAL_NC)
#define S2_BLOCK_DEVICE			(S2_DES_BLOCK | S2_AF | S2_SH_OUTER | S2_MEMATTR_DEV_nGnRE | S2_XN)
// #define S2_BLOCK_DEVICE		(S2_DES_BLOCK | S2_AF | S2_SH_OUTER | S2_MEMATTR_DEV_nGnRnE | S2_XN)
#define S2_BLOCK_WT			(S2_DES_BLOCK | S2_AF | S2_SH_OUTER | S2_MEMATTR_NORMAL_WT)

/*
 * 40bit stage2 IPA use 3 levels page table, the pagetable
 * need 2 pages
 */
#define STAGE2_PGTABLE_LEVELS		3
#define S2_PAGETABLE_SIZE		8192

#define S2_PHYSICAL_MASK		0x0000fffffffff000UL
#define S2_PHYSICAL_MAX			(1UL << 40)
#define S2_VIRT_MAX			(1UL << 40)
#define S2_PHYSICAL_SIZE		(1UL << 40)

#define GUEST_PGD_PAGES			2
#define GUEST_PGD_PAGE_ALIGN		2

#define S2_PGD_SHIFT			39
#define S2_PGD_SIZE			(1UL << S2_PGD_SHIFT)
#define S2_PGD_MASK			(~(S2_PGD_SIZE - 1))

#define S2_PUD_SHIFT			30
#define S2_PUD_SIZE			(1UL << S2_PUD_SHIFT)
#define S2_PUD_MASK			(~(S2_PUD_SIZE - 1))

#define S2_PMD_SHIFT			21
#define S2_PMD_SIZE			(1UL << S2_PMD_SHIFT)
#define S2_PMD_MASK			(~(S2_PMD_SIZE - 1))

#define S2_PTE_SHIFT			12
#define S2_PTE_SIZE			(1UL << S2_PTE_SHIFT)
#define S2_PTE_MASK			(~(S2_PTE_SIZE - 1))

/*
 * The number of PTRS across all concatenated stage2 tables given by the
 * number of bits resolved at the initial level.
 *
 * since we use 3 level pagetable, and translation walk will from pud, the
 * PTRS in one pud is 1024
 */
#define PTRS_PER_S2_PGD			512
#define PTRS_PER_S2_PUD			1024
#define PTRS_PER_S2_PMD			512
#define PTRS_PER_S2_PTE			512

#define stage2_pud_value(pudp)		(*(pudp))
#define stage2_pmd_value(pmdp)		(*(pmdp))
#define stage2_pte_value(ptep)		(*(ptep))

#define IS_PUD_ALIGN(addr)\
	(!((unsigned long)(addr) & (S2_PUD_SIZE - 1)))

#define stage2_pgd_index(addr)		(((addr) >> S2_PGD_SHIFT) & (PTRS_PER_S2_PGD - 1))
#define stage2_pud_index(addr)		(((addr) >> S2_PUD_SHIFT) & (PTRS_PER_S2_PUD - 1))
#define stage2_pmd_index(addr)		(((addr) >> S2_PMD_SHIFT) & (PTRS_PER_S2_PMD - 1))
#define stage2_pte_index(addr)		(((addr) >> S2_PTE_SHIFT) & (PTRS_PER_S2_PTE - 1))

#define stage2_pgd_offset(pgdp, addr)		((pgd_t *)(pgdp) + stage2_pgd_index((unsigned long)addr))
#define stage2_pud_offset(pudp, addr)		((pud_t *)(pudp) + stage2_pud_index((unsigned long)addr))
#define stage2_pmd_offset(pmdp, addr)		((pmd_t *)(pmdp) + stage2_pmd_index((unsigned long)addr))
#define stage2_pte_offset(ptep, addr)		((pte_t *)(ptep) + stage2_pte_index((unsigned long)addr))

#define stage2_pud_huge(pud)			((pud) && ((pud) & 0x03) == S2_DES_BLOCK)
#define stage2_pmd_huge(pmd)			((pmd) && ((pmd) & 0x03) == S2_DES_BLOCK)

#define stage2_pud_table_addr(pgd)		(pud_t *)(unsigned long)((pgd) & S2_PHYSICAL_MASK)
#define stage2_pmd_table_addr(pud)		(pmd_t *)(unsigned long)((pud) & S2_PHYSICAL_MASK)
#define stage2_pte_table_addr(pmd)		(pte_t *)(unsigned long)((pmd) & S2_PHYSICAL_MASK)

#define stage2_pgd_none(pgd)			((pgd) == 0)
#define stage2_pud_none(pud)			((pud) == 0)
#define stage2_pmd_none(pmd)			((pmd) == 0)
#define stage2_pte_none(pte)			((pte) == 0)

#define stage2_pmd_pfn(pmd)			(pmd >> S2_PMD_SHIFT)
#define stage2_pfn_pte(pfn, attr)		(((unsigned long)(pfn) << PAGE_SHIFT) | attr)

#define S2_PTE_ADDR_MASK			(0x0000fffffffff000UL)
#define S2_PMD_ADDR_MASK			(0x0000ffffffe00000UL)

#endif
