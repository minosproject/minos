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

static inline unsigned long arch_guest_tt_description(unsigned long flags)
{
	unsigned long attr;
	unsigned long d_type, m_type, rw;

	d_type = flags & VM_DES_MASK;
	m_type = flags & VM_TYPE_MASK;
	rw = flags & VM_RW_MASK;

	switch (m_type) {
	case VM_NORMAL:
		attr = S2_MEMATTR_NORMAL_WB | S2_AF | S2_SH_INNER;
		break;
	case VM_NORMAL_NC:
		attr = S2_MEMATTR_NORMAL_NC | S2_AF | S2_SH_INNER | S2_XN;
		break;
	case VM_IO:
		attr = S2_MEMATTR_DEV_nGnRnE | S2_AF | S2_XN;
		break;
	default:
		attr = S2_MEMATTR_NORMAL_WB | S2_AF | S2_SH_INNER;
		break;
	}

	if (d_type == VM_DES_BLOCK)
		attr |= S2_DES_BLOCK;
	else
		attr |= S2_DES_PAGE;

	if (rw == VM_RO)
		attr |= S2_S2AP_RO;
	else if (rw == VM_WO)
		attr |= S2_S2AP_WO;
	else if (rw == VM_RW_NON)
		attr |= S2_S2AP_NON;
	else
		attr |= S2_S2AP_RW;

	return attr;
}

static inline unsigned long arch_host_tt_description(unsigned long flags)
{
	unsigned long attr = 0;
	unsigned long d_type, m_type, rw;

	d_type = flags & VM_DES_MASK;
	m_type = flags & VM_TYPE_MASK;
	rw = flags & VM_RW_MASK;

	if (d_type == VM_DES_TABLE)
		return (unsigned long)S1_DES_TABLE;

	switch (m_type) {
	case VM_NORMAL:
		attr = S1_ATTR_IDX(MT_NORMAL) | S1_NS |
			S1_SH_INNER | S1_AF;
		break;
	case VM_NORMAL_NC:
		attr = S1_ATTR_IDX(MT_NORMAL_NC) | S1_NS |
			S1_SH_INNER | S1_AF | S1_XN;
		break;
	case VM_IO:
		attr = S1_ATTR_IDX(MT_DEVICE_nGnRnE) | S1_NS |
			S1_AF | S1_XN;
		break;
	default:
		attr = S1_ATTR_IDX(MT_NORMAL) | S1_NS |
			S1_SH_INNER | S1_AF;
		break;
	}

	if (d_type == VM_DES_BLOCK)
		attr |= S1_DES_BLOCK;
	else
		attr |= S1_DES_PAGE;

	if (rw == VM_RO)
		attr |= S1_AP_RO;
	else
		attr |= S1_AP_RW;

	return attr;
}

static inline unsigned long
arch_page_table_description(unsigned long flags)
{
	if (!!(flags & VM_HOST))
		return arch_host_tt_description(flags);
	else
		return arch_guest_tt_description(flags);
}

static inline int get_mapping_type(int lvl, unsigned long addr)
{
	unsigned long type = addr & 0x03;

	if (type == S1_DES_FAULT)
		return VM_DES_FAULT;

	if (lvl == PTE) {
		if (type != S1_DES_PAGE)
			return VM_DES_FAULT;
		else
			return VM_DES_PAGE;
	} else if (lvl == PMD) {
		if (type == S1_DES_TABLE)
			return VM_DES_TABLE;
		else if (type == S1_DES_BLOCK)
			return VM_DES_BLOCK;
		else
			return VM_DES_TABLE;
	} else if (lvl == PUD) {
		if (type == S1_DES_TABLE)
			return VM_DES_TABLE;
		else if (type == S1_DES_BLOCK)
			return VM_DES_BLOCK;
		else
			return VM_DES_TABLE;
	} else if (lvl == PGD) {
		if (type != S1_DES_TABLE)
			return VM_DES_FAULT;
		else
			return VM_DES_TABLE;
	}

	return VM_DES_FAULT;
}

#endif
