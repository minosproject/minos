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

#include <minos/types.h>
#include <minos/memattr.h>
#include <asm/asm_mmu.h>

static inline unsigned long arch_guest_tt_description(unsigned long flags)
{
	unsigned long attr;
	unsigned long d_type, m_type, rw;

	d_type = flags & VM_DES_MASK;
	m_type = flags & VM_TYPE_MASK;
	rw = flags & VM_RW_MASK;

	if (d_type == VM_DES_TABLE)
		return (unsigned long)S2_DES_TABLE;

	switch (m_type) {
	case VM_NORMAL:
		attr = S2_MEMATTR_NORMAL_WB | S2_AF | S2_SH_INNER;
		break;
	case VM_NORMAL_NC:
		attr = S2_MEMATTR_NORMAL_NC | S2_AF | S2_SH_INNER | S2_XN;
		break;
	case VM_IO:
		attr = S2_MEMATTR_DEV_nGnRnE | S2_AF | S2_SH_OUTER | S2_XN;
		break;
	case VM_NORMAL_XN:
		attr = S2_MEMATTR_NORMAL_WB | S2_AF | S2_SH_INNER | S2_XN;
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
		attr = S1_ATTR_IDX(MT_NORMAL) | S1_NS | S1_SH_INNER | S1_AF;
		break;
	case VM_NORMAL_NC:
		attr = S1_ATTR_IDX(MT_NORMAL_NC) | S1_NS | S1_SH_INNER | S1_AF | S1_XN;
		break;
	case VM_IO:
		attr = S1_ATTR_IDX(MT_DEVICE_nGnRnE) | S1_NS | S1_SH_OUTER | S1_AF | S1_XN;
		break;
	case VM_NORMAL_XN:
		attr = S1_ATTR_IDX(MT_NORMAL) | S1_NS | S1_AF | S1_SH_INNER | S1_XN;
		break;
	default:
		attr = S1_ATTR_IDX(MT_NORMAL) | S1_NS | S1_SH_INNER | S1_AF;
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

unsigned long arch_page_table_description(unsigned long flags)
{
	if (!!(flags & VM_HOST))
		return arch_host_tt_description(flags);
	else
		return arch_guest_tt_description(flags);
}

int get_mapping_type(int lvl, unsigned long addr)
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
