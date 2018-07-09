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
#include <config/config.h>
#include <minos/vcpu.h>
#include <minos/vmodule.h>
#include <asm/arch.h>
#include <minos/mm.h>

/*
 * a - Non-shareable
 * 	This represents memory accessible only by a single processor or other
 * 	agent, so memory accesses never need to be synchronized with other
 * 	processors. This domain is not typically used in SMP systems.
 * b - Inner shareable
 * 	This represents a shareability domain that can be shared by multiple
 * 	processors, but not necessarily all of the agents in the system. A
 * 	system might have multiple Inner Shareable domains. An operation that
 * 	affects one Inner Shareable domain does not affect other Inner Shareable
 * 	domains in the system. An example of such a domain might be a quad-core
 * 	Cortex-A57 cluster.
 * c - Outer shareable
 * 	An outer shareable (OSH) domain re-orderis shared by multiple agents
 * 	and can consist of one or more inner shareable domains. An operation
 * 	that affects an outer shareable domain also implicitly affects all inner
 * 	shareable domains inside it. However, it does not otherwise behave as an
 * 	inner shareable operation.
 * d - Full system
 * 	An operation on the full system (SY) affects all observers in the system.
 */

struct vmsa_context {
	uint64_t vtcr_el2;
	uint64_t ttbr0_el1;
	uint64_t ttbr1_el1;
	uint64_t vttbr_el2;
	uint64_t mair_el1;
	uint64_t tcr_el1;
}__align(sizeof(unsigned long));

struct aa64mmfr0 {
	uint64_t pa_range : 4;
	uint64_t asid : 4;
	uint64_t big_end : 4;
	uint64_t sns_mem : 4;
	uint64_t big_end_el0 : 4;
	uint64_t t_gran_16k : 4;
	uint64_t t_gran_64k : 4;
	uint64_t t_gran_4k : 4;
	uint64_t res : 32;
};

struct tt_lvl_config {
	int range_offset;
	int des_offset;
	unsigned long offset_mask;
	size_t map_size;
	struct tt_lvl_config *next;
	struct tt_lvl_config *prev;
};

struct map_info {
	unsigned long table_base;
	unsigned long vir_base;
	unsigned long phy_base;
	size_t size;
	int lvl;
	int host;
	int mem_type;
	struct tt_lvl_config *config;
};

extern unsigned char __el2_ttb0_l0;
static unsigned long el2_ttb0_l0;

static spinlock_t host_lock;

#define G4K_LVL0_OFFSET			(39)
#define G4K_LVL1_OFFSET			(30)
#define G4K_LVL2_OFFSET			(21)
#define G4K_LVL3_OFFSET			(12)
#define G4K_DESCRIPTION_OFFSET		(12)

#define VMSA_DESC_MASK(n) (~((1UL << (n)) - 1))

struct tt_lvl_config g4k_host_lvl3_config = {
	.range_offset	= 12,
	.des_offset	= 12,
	.offset_mask	= 0x1ffUL << 12,
	.map_size	= SIZE_4K,
	.next		= NULL,
};

struct tt_lvl_config g4k_host_lvl2_config = {
	.range_offset	= 21,
	.des_offset	= 12,
	.offset_mask	= 0x1ffUL << 21,
	.map_size	= SIZE_2M,
	.next		= &g4k_host_lvl3_config,
};

struct tt_lvl_config g4k_host_lvl1_config = {
	.range_offset	= 30,
	.des_offset	= 12,
	.offset_mask	= 0x1ffUL << 30,
	.map_size	= SIZE_1G,
	.next		= &g4k_host_lvl2_config,
};

struct tt_lvl_config g4k_host_lvl0_config = {
	.range_offset	= 39,
	.des_offset	= 12,
	.offset_mask	= 0x1ffUL << 39,
	.map_size	= (512 * SIZE_1G),
	.next		= &g4k_host_lvl1_config,
};

struct tt_lvl_config g4k_guest_lvl3_config = {
	.range_offset	= 12,
	.des_offset	= 12,
	.offset_mask	= 0x1ffUL << 12,
	.map_size	= SIZE_4K,
	.next		= NULL,
};

struct tt_lvl_config g4k_guest_lvl2_config = {
	.range_offset	= 21,
	.des_offset	= 12,
	.offset_mask	= 0x1ffUL << 21,
	.map_size	= SIZE_2M,
	.next		= &g4k_guest_lvl3_config,
};

struct tt_lvl_config g4k_guest_lvl1_config = {
	.range_offset	= 30,
	.des_offset	= 12,
	.offset_mask	= 0x3ffUL << 30,
	.map_size	= SIZE_1G,
	.next		= &g4k_guest_lvl2_config,
};

static unsigned long guest_tt_description(int m_type, int d_type)
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

static unsigned long host_tt_description(int m_type, int d_type)
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

static inline unsigned long get_tt_description(int host, int m_type, int d_type)
{
	if (host)
		return host_tt_description(m_type, d_type);
	else
		return guest_tt_description(m_type, d_type);
}

static int get_map_type(struct map_info *info)
{
	struct tt_lvl_config *config = info->config;
	unsigned long a, b;

	if (info->lvl == 3)
		return DESCRIPTION_PAGE;

	/*
	 * check whether the map size is level map
	 * size align and the virtual base aligin
	 * FIX ME : whether need to check the physical base?
	 */
	a = (info->size) & (config->map_size - 1);
	b = (info->vir_base) & (config->map_size - 1);
	if (a || b)
		return DESCRIPTION_TABLE;
	else
		return DESCRIPTION_BLOCK;
}

static int create_page_entry(struct map_info *info)
{
	int i, map_type;
	uint32_t offset, index;
	unsigned long attr;
	struct tt_lvl_config *config = info->config;
	unsigned long *tbase = (unsigned long *)info->table_base;

	if (info->lvl != 3)
		map_type = DESCRIPTION_BLOCK;
	else
		map_type = DESCRIPTION_PAGE;

	offset = config->range_offset;
	attr = get_tt_description(info->host, info->mem_type, map_type);

	index = (info->vir_base & config->offset_mask) >> offset;

	for (i = 0; i < (info->size >> offset); i++) {
		*(tbase + index) = attr | (info->phy_base &
				VMSA_DESC_MASK(config->des_offset));
		info->vir_base += config->map_size;
		info->phy_base += config->map_size;
		index++;
	}

	return 0;
}

static int create_table_entry(struct map_info *info)
{
	size_t size, map_size;
	unsigned long attr, value, offset;
	int ret = 0, map_type, new_page;
	struct map_info map_info;
	struct tt_lvl_config *config = info->config;
	unsigned long *tbase = (unsigned long *)info->table_base;

	size = info->size;
	attr = get_tt_description(info->host,
			info->mem_type, DESCRIPTION_TABLE);

	while (size > 0) {
		new_page = 0;
		map_size = BALIGN(info->vir_base, config->map_size) - info->vir_base;
		map_size = map_size ? map_size : config->map_size;
		if (map_size > size)
			map_size = size;

		offset = (info->vir_base & config->offset_mask) >>
			config->range_offset;
		value = *(tbase + offset);

		if (value == 0) {
			value = (unsigned long)get_free_page();
			if (!value)
				return -ENOMEM;

			new_page = 1;
			memset((void *)value, 0, SIZE_4K);
			*(tbase + offset) = attr | (value &
					VMSA_DESC_MASK(config->des_offset));
			flush_dcache_range((unsigned long)(tbase + offset),
						sizeof(unsigned long));
		} else {
			/* get the base address of the entry */
			value = value & 0x0000ffffffffffff;
			value = value >> config->des_offset;
			value = value << config->des_offset;
		}

		memset(&map_info, 0, sizeof(struct map_info));
		map_info.table_base = value;
		map_info.vir_base = info->vir_base;
		map_info.phy_base = info->phy_base;
		map_info.size = map_size;
		map_info.lvl = info->lvl + 1;
		map_info.mem_type = info->mem_type;
		map_info.host = info->host;
		map_info.config = config->next;

		/*
		 * get next level map entry type, if the entry
		 * has been already maped then force it to a
		 * Table description
		 *
		 * FIX ME: if the attribute of the page table entry
		 * is changed, such as from TABLE to BLOCK, need to
		 * free the page table page --- TBD
		 */
		map_type = get_map_type(&map_info);

		if (map_type == DESCRIPTION_TABLE)
			ret = create_table_entry(&map_info);
		else
			ret = create_page_entry(&map_info);
		if (ret) {
			if (new_page) {
				free((void *)value);
				new_page = 0;
			}

			return ret;
		}

		flush_dcache_range(value, SIZE_4K);
		info->vir_base += map_size;
		size -= map_size;
		info->phy_base += map_size;
	}

	return ret;
}

static unsigned long alloc_guest_pt(void)
{
	/*
	 * return the table base address, this function
	 * is called when init the vm
	 *
	 * 2 pages for each VM to map 1T IPA memory
	 *
	 */
	void *page;

	page = get_free_pages(2);
	if (!page)
		panic("No memory to map vm memory\n");

	memset(page, 0, SIZE_4K * 2);
	return (unsigned long)page;
}


static int map_memory(struct map_info *info)
{
	int ret;

	ret = create_table_entry(info);
	if (ret)
		pr_error("map host 0x%x->0x%x size:%x failed\n",
				info->vir_base, info->phy_base, info->size);
	else
		flush_all_tlb();

	return ret;
}

int map_host_mem(unsigned long vir, unsigned long phy, size_t size, int type)
{
	int ret = 0;
	unsigned long vir_base, phy_base, tmp;
	struct map_info map_info;

	vir_base = ALIGN(vir, SIZE_4K);
	phy_base = ALIGN(phy, SIZE_4K);
	tmp = BALIGN(vir_base + size, SIZE_4K);
	size = tmp - vir_base;

	memset(&map_info, 0, sizeof(struct map_info));
	map_info.table_base = el2_ttb0_l0;
	map_info.vir_base = vir_base;
	map_info.phy_base = phy_base;
	map_info.size = size;
	map_info.lvl = 0;
	map_info.host = 1;
	map_info.mem_type = type;
	map_info.config = &g4k_host_lvl0_config;

	spin_lock(&host_lock);
	ret = map_memory(&map_info);
	spin_unlock(&host_lock);

	return ret;
}

int map_guest_mem(unsigned long tt, unsigned long vir,
		unsigned long phy, size_t size, int type)
{
	unsigned long vir_base, phy_base, tmp;
	struct map_info map_info;

	vir_base = ALIGN(vir, SIZE_4K);
	phy_base = ALIGN(phy, SIZE_4K);
	tmp = BALIGN(vir_base + size, SIZE_4K);
	size = tmp - vir_base;

	memset(&map_info, 0, sizeof(struct map_info));
	map_info.table_base = tt;
	map_info.vir_base = vir_base;
	map_info.phy_base = phy_base;
	map_info.size = size;
	map_info.lvl = 1;
	map_info.host = 0;
	map_info.mem_type = type;
	map_info.config = &g4k_guest_lvl1_config;

	return map_memory(&map_info);
}

static uint64_t generate_vtcr_el2(void)
{
	uint64_t value = 0;

	/*
	 * vtcr_el2 used to defined the memory attribute
	 * for the EL1, this is defined by hypervisor
	 * and may do not related to physical information
	 */
	value |= (24 << 0);	// t0sz = 0x10 40bits vaddr
	value |= (0x01 << 6);	// SL0: 4kb start at level1
	value |= (0x1 << 8);	// Normal memory, Inner WBWA
	value |= (0x1 << 10);	// Normal memory, Outer WBWA
	value |= (0x3 << 12);	// Inner Shareable

	// TG0 4K
	value |= (0x0 << 14);

	// PS --- pysical size 1TB
	value |= (2 << 16);

	// vmid -- 8bit
	value |= (0x0 << 19);

	return value;
}

static uint64_t generate_vttbr_el2(uint32_t vmid, unsigned long base)
{
	uint64_t value = 0;

	value = base ;
	value |= (uint64_t)vmid << 48;

	return value;
}

int el2_stage1_init(void)
{
	el2_ttb0_l0 = (unsigned long)&__el2_ttb0_l0;
	g4k_host_lvl0_config.prev = NULL;
	g4k_host_lvl1_config.prev = &g4k_host_lvl0_config;
	g4k_host_lvl2_config.prev = &g4k_host_lvl1_config;
	g4k_host_lvl3_config.prev = &g4k_host_lvl1_config;
	g4k_guest_lvl1_config.prev = NULL;
	g4k_guest_lvl2_config.prev = &g4k_guest_lvl1_config;
	g4k_guest_lvl3_config.prev = &g4k_guest_lvl2_config;
	spin_lock_init(&host_lock);

	return 0;
}

int el2_stage2_init(void)
{
	/*
	 * now just support arm fvp, TBD to support more
	 * platform
	 */
	uint64_t value, dcache, icache;
	struct aa64mmfr0 aa64mmfr0;

	value = read_id_aa64mmfr0_el1();
	memcpy(&aa64mmfr0, &value, sizeof(uint64_t));
	pr_debug("aa64mmfr0: pa_range:0x%x"
		 " t_gran_16k:0x%x t_gran_64k"
		 ":0x%x t_gran_4k:0x%x\n",
		 aa64mmfr0.pa_range, aa64mmfr0.t_gran_16k,
		aa64mmfr0.t_gran_64k, aa64mmfr0.t_gran_4k);

	value = read_sysreg(CTR_EL0);
	dcache = 4 << ((value & 0xf0000) >> 16);
	icache = 4 << ((value & 0xf));
	pr_info("dcache_line_size:%d ichache_line_size:%d\n", dcache, icache);

	return 0;
}

static void vmsa_state_init(struct vcpu *vcpu, void *context)
{
	struct vm *vm = vcpu->vm;
	struct vmsa_context *c = (struct vmsa_context *)context;

	c->vtcr_el2 = generate_vtcr_el2();
	c->vttbr_el2 = generate_vttbr_el2(vm->vmid, vm->mm.page_table_base);
	c->ttbr0_el1 = 0;
	c->ttbr1_el1 = 0;
	c->mair_el1 = 0;
	c->tcr_el1 = 0;
}

static void vmsa_state_save(struct vcpu *vcpu, void *context)
{
	struct vmsa_context *c = (struct vmsa_context *)context;

	dsb();
	c->vtcr_el2 = read_sysreg(VTCR_EL2);
	c->vttbr_el2 = read_sysreg(VTTBR_EL2);
	c->ttbr0_el1 = read_sysreg(TTBR0_EL1);
	c->ttbr1_el1 = read_sysreg(TTBR1_EL1);
	c->mair_el1 = read_sysreg(MAIR_EL1);
	c->tcr_el1 = read_sysreg(TCR_EL1);
}

static void vmsa_state_restore(struct vcpu *vcpu, void *context)
{
	struct vmsa_context *c = (struct vmsa_context *)context;

	write_sysreg(c->vtcr_el2, VTCR_EL2);
	write_sysreg(c->vttbr_el2, VTTBR_EL2);
	write_sysreg(c->ttbr0_el1, TTBR0_EL1);
	write_sysreg(c->ttbr1_el1, TTBR1_EL1);
	write_sysreg(c->mair_el1, MAIR_EL1);
	write_sysreg(c->tcr_el1, TCR_EL1);
	dsb();
	flush_local_tlb();
}

static struct mmu_chip vmsa_mmu = {
	.map_guest_memory	= map_guest_mem,
	.map_host_memory 	= map_host_mem,
	.alloc_guest_pt 	= alloc_guest_pt,
};

static int vmsa_vmodule_init(struct vmodule *vmodule)
{
	vmodule->context_size = sizeof(struct vmsa_context);
	vmodule->pdata = NULL;
	vmodule->state_init = vmsa_state_init;
	vmodule->state_save = vmsa_state_save;
	vmodule->state_restore = vmsa_state_restore;

	return 0;
}

MINOS_MODULE_DECLARE(vmsa, "armv8-mmu", (void *)vmsa_vmodule_init);
MMUCHIP_DECLARE(armv8_mmu, "armv8-mmu", (void *)&vmsa_mmu);
