#ifndef __MINOS_ARCH_PAGETABLE_H__
#define __MINOS_ARCH_PAGETABLE_H__

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

#define __GUEST_PGD_PAGE_NR		(2)
#define __GUEST_PGD_PAGE_ALIGN		(2)

#define __PAGETABLE_ATTR_MASK		(0x0000ffffffe00000UL)

static inline unsigned long arch_guest_tt_description(int m_type, int d_type)
{
	uint64_t attr;

	if (d_type == DESCRIPTION_TABLE)
		return (uint64_t)TT_S2_ATTR_TABLE;

	if (d_type == DESCRIPTION_BLOCK) {
		if (m_type == MEM_TYPE_NORMAL) {
			attr = TT_S2_ATTR_BLOCK | TT_S2_ATTR_AP_RW | \
			       TT_S2_ATTR_SH_INNER | TT_S2_ATTR_AF | \
			       TT_S2_ATTR_MEMATTR_OUTER_WB | \
			       TT_S2_ATTR_MEMATTR_NORMAL_INNER_WB;
		} else {
			attr = TT_S2_ATTR_BLOCK | TT_S2_ATTR_AP_RW | \
			       TT_S2_ATTR_SH_INNER | TT_S2_ATTR_AF | \
			       TT_S2_ATTR_XN | TT_S2_ATTR_MEMATTR_DEVICE | \
			       TT_S2_ATTR_MEMATTR_DEV_nGnRnE;
		}

		return attr;
	}

	if (d_type == DESCRIPTION_PAGE) {
		if (m_type == MEM_TYPE_NORMAL) {
			attr = TT_S2_ATTR_PAGE | TT_S2_ATTR_AP_RW | \
			       TT_S2_ATTR_SH_INNER | TT_S2_ATTR_AF | \
			       TT_S2_ATTR_MEMATTR_OUTER_WB | \
			       TT_S2_ATTR_MEMATTR_NORMAL_INNER_WB;
		} else {
			attr = TT_S2_ATTR_PAGE | TT_S2_ATTR_AP_RW | \
			       TT_S2_ATTR_SH_INNER | TT_S2_ATTR_AF | \
			       TT_S2_ATTR_XN | TT_S2_ATTR_MEMATTR_DEVICE | \
			       TT_S2_ATTR_MEMATTR_DEV_nGnRnE;
		}

		return attr;
	}

	return 0;
}

static inline unsigned long arch_host_tt_description(int m_type, int d_type)
{
	uint64_t attr;

	if (d_type == DESCRIPTION_TABLE)
		return (uint64_t)TT_S1_ATTR_TABLE;

	if (d_type == DESCRIPTION_BLOCK) {
		if (m_type == MEM_TYPE_NORMAL) {
			attr = TT_S1_ATTR_BLOCK | \
			       (1 << TT_S1_ATTR_MATTR_LSB) | \
			       TT_S1_ATTR_NS | \
			       TT_S1_ATTR_AP_RW_PL1 | \
			       TT_S1_ATTR_SH_INNER | \
			       TT_S1_ATTR_AF | \
			       TT_S1_ATTR_nG;
		} else {
			attr = TT_S1_ATTR_BLOCK | \
			       (2 << TT_S1_ATTR_MATTR_LSB) | \
			       TT_S1_ATTR_NS | \
			       TT_S1_ATTR_AP_RW_PL1 | \
			       TT_S1_ATTR_AF | \
			       TT_S1_ATTR_XN | \
			       TT_S1_ATTR_nG;
		}

		return attr;
	}

	if (d_type == DESCRIPTION_PAGE) {
		if (m_type == MEM_TYPE_NORMAL) {
			attr = TT_S1_ATTR_PAGE | \
			       (1 << TT_S1_ATTR_MATTR_LSB) | \
			       TT_S1_ATTR_NS | \
			       TT_S1_ATTR_AP_RW_PL1 | \
			       TT_S1_ATTR_SH_INNER | \
			       TT_S1_ATTR_AF | \
			       TT_S1_ATTR_nG;
		} else {
			attr = TT_S1_ATTR_PAGE | \
			       (2 << TT_S1_ATTR_MATTR_LSB) | \
			       TT_S1_ATTR_NS | \
			       TT_S1_ATTR_AP_RW_PL1 | \
			       TT_S1_ATTR_AF | \
			       TT_S1_ATTR_XN | \
			       TT_S1_ATTR_nG;
		}

		return attr;
	}

	return 0;
}

static inline int get_mapping_type(int lvl, unsigned long addr)
{
	unsigned long type = addr & 0x03;

	if (lvl == PTE) {
		if (type != TT_S1_ATTR_PAGE)
			return DESCRIPTION_FAULT;
		else
			return DESCRIPTION_PAGE;
	} else if (lvl == PMD) {
		if (type == TT_S1_ATTR_PAGE)
			return DESCRIPTION_FAULT;
		else if (type == TT_S1_ATTR_BLOCK)
			return DESCRIPTION_BLOCK;
		else
			return DESCRIPTION_TABLE;
	} else if (lvl == PUD) {
		if (type == TT_S1_ATTR_PAGE)
			return DESCRIPTION_FAULT;
		else if (type == TT_S1_ATTR_BLOCK)
			return DESCRIPTION_BLOCK;
		else
			return DESCRIPTION_TABLE;
	} else if (lvl == PGD) {
		if (type != TT_S1_ATTR_TABLE)
			return DESCRIPTION_FAULT;
		else
			return DESCRIPTION_PAGE;
	}

	return DESCRIPTION_FAULT;
}

#endif
