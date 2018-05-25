#include <minos/minos.h>
#include <minos/mmu.h>
#include <config/config.h>
#include <virt/vcpu.h>
#include <virt/vmodule.h>
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
}__align(sizeof(unsigned long));

/*
 * now using two level page table in host
 * mode and guest mode, host using 4k granule
 * size and guest using 64k granule size
 */
struct mmu_config {
	uint32_t table_offset;
	uint32_t level1_table_size;
	uint32_t level1_entry_map_size;
	uint32_t level1_offset;
	int level1_description_type;
	uint32_t level2_pages;
	uint32_t level2_entry_map_size;
	uint32_t level2_offset;
	int level2_description_type;
	char *(*alloc_level2_pages)(int pages);
	uint64_t (*get_tt_description)(int mtype, int dtype);
	spinlock_t *lock;
};

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

#define G4K_LEVEL1_OFFSET	(30)
#define G4K_LEVEL2_OFFSET	(21)
#define G16K_LEVEL1_OFFSET	(25)
#define G16K_LEVEL2_OFFSET	(14)
#define G64K_LEVEL1_OFFSET	(29)
#define G64K_LEVEL2_OFFSET	(16)

#ifdef CONFIG_GRANULE_SIZE_4K
	#define GRANULE_TYPE		GRANULE_SIZE_4K
	#define LEVEL2_OFFSET		G4K_LEVEL2_OFFSET
	#define LEVEL1_OFFSET		G4K_LEVEL1_OFFSET
	#define LEVEL2_ENTRY_MAP_SIZE	(SIZE_2M)
	#define LEVEL1_ENTRY_MAP_SIZE	(SIZE_1G)
	#define LEVEL1_DESCRIPTION_TYPE	(DESCRIPTION_TABLE)
	#define LEVEL2_DESCRIPTION_TYPE	(DESCRIPTION_BLOCK)
	#define LEVEL2_PAGES		(1)
#elif CONFIG_GRANULE_SIZE_16K
	#define GRANULE_TYPE		GRANULE_SIZE_16K
	#define LEVEL1_OFFSET		G16K_LEVEL1_OFFSET
	#define LEVEL2_OFFSET		G16K_LEVEL2_OFFSET
	#define LEVEL2_ENTRY_MAP_SIZE	(SIZE_16K)
	#define LEVEL1_ENTRY_MAP_SIZE	(SIZE_32M)
	#define LEVEL1_DESCRIPTION_TYPE	(DESCRIPTION_TABLE)
	#define LEVEL2_DESCRIPTION_TYPE	(DESCRIPTION_PAGE)
	#define LEVEL2_PAGES		(4)
#else
	#define GRANULE_TYPE		GRANULE_SIZE_64K
	#define LEVEL2_OFFSET		G64K_LEVEL2_OFFSET
	#define LEVEL1_OFFSET		G64K_LEVEL1_OFFSET
	#define LEVEL2_ENTRY_MAP_SIZE	(SIZE_64K)
	#define LEVEL1_ENTRY_MAP_SIZE	(SIZE_512M)
	#define LEVEL1_DESCRIPTION_TYPE	(DESCRIPTION_TABLE)
	#define LEVEL2_DESCRIPTION_TYPE	(DESCRIPTION_PAGE)
	#define LEVEL2_PAGES		(16)
#endif

extern unsigned char __el2_stage2_ttb_l1;
extern unsigned char __el2_stage2_ttb_l1_end;
extern unsigned char __el2_stage2_ttb_l2;
extern unsigned char __el2_stage2_ttb_l2_end;

extern unsigned char __el2_ttb0_l1;
extern unsigned char __el2_ttb0_l2_code;
static unsigned long el2_ttb0_l1;
static spinlock_t host_lock;

static char *level1_base;
static char *level2_base;
static size_t level1_size;
static size_t level2_size;

static spinlock_t guest_lock;

static struct mmu_config host_config;
static struct mmu_config guest_config;

static char *alloc_guest_level2(int pages)
{
	char *ret = NULL;

	if (level2_size < (pages * SIZE_4K))
		return NULL;

	ret = level2_base;
	level2_base += (pages * SIZE_4K);
	level2_size -= (pages * SIZE_4K);

	return ret;
}

static char *mmu_alloc_level1_mem()
{
	char *ret = NULL;

	spin_lock(&guest_lock);

	if (level1_size < MMU_TTB_LEVEL1_SIZE)
		return NULL;

	ret = level1_base;
	level1_base += MMU_TTB_LEVEL1_SIZE;
	level1_size -= MMU_TTB_LEVEL1_SIZE;

	spin_unlock(&guest_lock);

	return ret;
}

static uint64_t guest_tt_description(int m_type, int d_type)
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
			       TT_S2_ATTR_MEMATTR_DEVICE | \
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
			       TT_S2_ATTR_MEMATTR_DEVICE | \
			       TT_S2_ATTR_MEMATTR_DEV_nGnRnE;
		}

		return attr;
	}

	return 0;
}

static char *alloc_host_level2(int pages)
{
	return get_free_pages(pages);
}

static uint64_t host_tt_description(int m_type, int d_type)
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
			       TT_S1_ATTR_nG;
		}

		return attr;
	}

	return 0;
}

static int map_level2_pages(unsigned long *tbase, unsigned long vbase,
			unsigned long pbase, size_t size,
			int type, struct mmu_config *config)
{
	int i;
	uint64_t attr;
	uint32_t offset, index;
	unsigned long tmp;

	offset = config->level2_offset;
	attr = config->get_tt_description(type,
			config->level2_description_type);

	tmp = ALIGN(vbase, config->level1_entry_map_size);
	index = (vbase - tmp) >> offset;
	tmp = index;

	for (i = 0; i < (size >> offset); i++) {
		if (*(tbase + index) != 0)
			goto again;

		*(tbase + index) = (attr | \
			(pbase >> offset) << offset);

again:
		vbase += config->level2_entry_map_size;
		pbase += config->level2_entry_map_size;
		index++;
	}

	return 0;
}

static int map_mem(unsigned long t_base, unsigned long base,
		unsigned long phy_base, size_t size,
		int type, struct mmu_config *config)
{
	int ret = 0;
	unsigned long tmp;
	uint32_t offset;
	uint64_t value;
	uint64_t attr;
	size_t map_size;
	unsigned long *tbase = (unsigned long *)t_base;

	spin_lock(config->lock);

	attr = config->get_tt_description(type,
			config->level1_description_type);

	while (size > 0) {
		offset = base >> (config->level1_offset);
		value = *(tbase + offset);
		if (value == 0) {
			tmp = (unsigned long)config->alloc_level2_pages(
					config->level2_pages);
			if (!tmp) {
				ret =  -ENOMEM;
				goto out;
			}

			memset((char *)tmp, 0, config->level2_pages * SIZE_4K);

			*(tbase + offset) = attr | \
				((tmp >> config->table_offset) << \
				 config->table_offset);
			value = (uint64_t)tmp;
			flush_dcache_range((unsigned long)(tbase + offset),
					sizeof(unsigned long));
		} else {
			/* get the base address of the entry */
			value = value & 0x0000ffffffffffff;
			value = value >> config->table_offset;
			value = value << config->table_offset;
		}

		map_size = BALIGN(base, config->level1_entry_map_size) - base;
		map_size = map_size ? map_size :
				config->level1_entry_map_size;
		if (map_size > size)
			map_size = size;

		map_level2_pages((unsigned long *)value, base,
				phy_base, map_size, type, config);

		flush_dcache_range(value,
			config->level2_pages * SIZE_4K);

		base += map_size;
		size -= map_size;
		phy_base += map_size;
	}

out:
	spin_unlock(config->lock);
	return ret;
}

static unsigned long alloc_guest_pt(void)
{
	/*
	 * return the table base address, this function
	 * is called when init the vm
	 */
	char *page;

	page = mmu_alloc_level1_mem();
	if (!page)
		panic("No memory to map vm memory\n");

	memset(page, 0, MMU_TTB_LEVEL1_SIZE);
	return (unsigned long)page;
}

static int map_host_mem(unsigned long vir, unsigned long phy,
			size_t size, int type)
{
	int ret;
	unsigned long vir_base, phy_base, tmp;
	struct mmu_config *config = &host_config;

	vir_base = ALIGN(vir, config->level2_entry_map_size);
	phy_base = ALIGN(phy, config->level2_entry_map_size);
	tmp = BALIGN(vir_base + size, config->level2_entry_map_size);
	size = tmp - vir_base;

	ret = map_mem(el2_ttb0_l1, vir_base,
			phy_base, size, type, config);
	if (ret) {
		pr_error("map host 0x%x->0x%x size:%x failed\n",
				vir, phy, size);
	} else {
		flush_all_tlb();
		//inv_dcache_range(vir_base, size);
	}

	return ret;
}

int map_guest_mem(unsigned long tt, unsigned long vir,
		unsigned long phy, size_t size, int type)
{
	int ret;
	unsigned long vir_base, phy_base, tmp;
	struct mmu_config *config = &guest_config;

	vir_base = ALIGN(vir, config->level2_entry_map_size);
	phy_base = ALIGN(phy, config->level2_entry_map_size);
	tmp = BALIGN(vir_base + size, config->level2_entry_map_size);
	size = tmp - vir_base;

	ret = map_mem(tt, vir_base, phy_base, size, type, config);
	if (ret) {
		pr_error("map host 0x%x->0x%x size:%x failed\n",
				vir, phy, size);
	}

	return ret;
}

static uint64_t generate_vtcr_el2(void)
{
	uint64_t value = 0;

	value |= (0x20 << 0);	// t0sz = 0x20 32bits vaddr
	value |= (0x01 << 6);	// SL0: 64kb/16 start at level2 4k start at level1
	value |= (0x1 << 8);	// Normal memory, Inner WBWA
	value |= (0x1 << 10);	// Normal memory, Outer WBWA
	value |= (0x3 << 12);	// Inner Shareable

	// TG0
	if (GRANULE_TYPE == GRANULE_SIZE_4K)
		value |= (0x0 << 14);
	else if (GRANULE_TYPE == GRANULE_SIZE_16K)
		value |= (0x2 << 14);
	else
		value |= (0x1 << 14);

	// PS --- pysical size
	value |= (0x0 << 16);

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

static int stage2_tt_mem_init(void)
{
	level1_base = (char *)&__el2_stage2_ttb_l1;
	level2_base = (char *)&__el2_stage2_ttb_l2;
	level1_size = &__el2_stage2_ttb_l1_end - &__el2_stage2_ttb_l1;
	level2_size = &__el2_stage2_ttb_l2_end - &__el2_stage2_ttb_l2;

	pr_info("level1: 0x%x 0x%x level2:0x%x 0x%x\n",
			level1_base, level1_size, level2_base, level2_size);
	if ((level1_size <= 0) || (level2_size <= 0))
		panic("No memory for el2 stage2 translation table\n");

	spin_lock_init(&guest_lock);

	memset((char *)&guest_config, 0, sizeof(struct mmu_config));
	guest_config.table_offset = 16;
	guest_config.level1_entry_map_size = LEVEL1_ENTRY_MAP_SIZE;
	guest_config.level1_offset = LEVEL1_OFFSET;
	guest_config.level1_description_type = LEVEL1_DESCRIPTION_TYPE;
	guest_config.level2_pages = LEVEL2_PAGES;
	guest_config.level2_entry_map_size = LEVEL2_ENTRY_MAP_SIZE;
	guest_config.level2_offset = LEVEL2_OFFSET;
	guest_config.level2_description_type = LEVEL2_DESCRIPTION_TYPE;
	guest_config.alloc_level2_pages = alloc_guest_level2;
	guest_config.get_tt_description = guest_tt_description;
	guest_config.lock = &guest_lock;
	guest_config.level1_table_size = 64 * 1024;

	return 0;
}

int el2_stage1_init(void)
{
	el2_ttb0_l1 = (unsigned long)&__el2_ttb0_l1;
	spin_lock_init(&host_lock);

	memset((char *)&host_config, 0, sizeof(struct mmu_config));

	host_config.table_offset = 12;
	host_config.level1_entry_map_size = SIZE_1G;
	host_config.level1_offset = G4K_LEVEL1_OFFSET;
	host_config.level1_description_type = DESCRIPTION_TABLE;
	host_config.level2_pages = 1;
	host_config.level2_entry_map_size = SIZE_2M;
	host_config.level2_offset = 21;
	host_config.level2_description_type = DESCRIPTION_BLOCK;
	host_config.alloc_level2_pages = alloc_host_level2;
	host_config.get_tt_description = host_tt_description;
	host_config.lock = &host_lock;
	host_config.level1_table_size = 4 * 1024;

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

	stage2_tt_mem_init();

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
}

static void vmsa_state_save(struct vcpu *vcpu, void *context)
{
	struct vmsa_context *c = (struct vmsa_context *)context;

	c->vtcr_el2 = read_sysreg(VTCR_EL2);
	c->vttbr_el2 = read_sysreg(VTTBR_EL2);
	c->ttbr0_el1 = read_sysreg(TTBR0_EL1);
	c->ttbr1_el1 = read_sysreg(TTBR1_EL1);
}

static void vmsa_state_restore(struct vcpu *vcpu, void *context)
{
	struct vmsa_context *c = (struct vmsa_context *)context;

	write_sysreg(c->vtcr_el2, VTCR_EL2);
	write_sysreg(c->vttbr_el2, VTTBR_EL2);
	write_sysreg(c->ttbr0_el1, TTBR0_EL1);
	write_sysreg(c->ttbr1_el1, TTBR1_EL1);
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
