#include <mvisor/mvisor.h>
#include <mvisor/mmu.h>
#include <config/config.h>
#include <mvisor/vcpu.h>
#include <mvisor/module.h>
#include <asm/arch.h>

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
} __attribute__ ((__aligned__ (sizeof(unsigned long))));

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

extern unsigned char __el2_stage2_ttb_l1;
extern unsigned char __el2_stage2_ttb_l1_end;
extern unsigned char __el2_stage2_ttb_l2;
extern unsigned char __el2_stage2_ttb_l2_end;

static char *level1_base;
static char *level2_base;
static size_t level1_size;
static size_t level2_size;

static spinlock_t tt_lock;

static int stage2_tt_mem_init(void)
{
	level1_base = &__el2_stage2_ttb_l1;
	level2_base = &__el2_stage2_ttb_l2;
	level1_size = &__el2_stage2_ttb_l1_end - &__el2_stage2_ttb_l1;
	level2_size = &__el2_stage2_ttb_l2_end - &__el2_stage2_ttb_l2;

	pr_info("level1: 0x%x 0x%x level2:0x%x 0x%x\n",
			level1_base, level1_size, level2_base, level2_size);
	if ((level1_size <= 0) || (level2_size <= 0))
		panic("No memory for el2 stage2 translation table\n");

	spin_lock_init(&tt_lock);

	return 0;
}

static char *mmu_alloc_level1_mem(void)
{
	char *ret = NULL;

	spin_lock(&tt_lock);
	if (level1_size < MMU_TTB_LEVEL1_SIZE)
		return NULL;

	ret = level1_base;
	level1_base += MMU_TTB_LEVEL1_SIZE;
	level1_size -= MMU_TTB_LEVEL1_SIZE;
	spin_unlock(&tt_lock);

	return ret;
}

static char *mmu_alloc_level2_mem(void)
{
	char *ret = NULL;

	spin_lock(&tt_lock);
	if (level2_size < (LEVEL2_PAGES * SIZE_4K))
		return NULL;

	ret = level2_base;
	level2_base += (LEVEL2_PAGES * SIZE_4K);
	level2_size -= (LEVEL2_PAGES * SIZE_4K);
	spin_unlock(&tt_lock);

	return ret;
}

uint64_t get_tt_description(int m_type, int d_type)
{
	uint64_t attr;

	if (d_type == DESCRIPTION_TABLE)
		return (uint64_t)TT_S2_ATTR_TABLE;

	if (d_type == DESCRIPTION_BLOCK) {
		if (m_type == MEM_TYPE_NORMAL) {
			attr = TT_S2_ATTR_BLOCK | TT_S2_ATTR_AP_RW | \
				TT_S2_ATTR_SH_INNER | TT_S2_ATTR_AF | \
				TT_S2_ATTR_MEMATTR_OUTER_WT | \
				TT_S2_ATTR_MEMATTR_NORMAL_INNER_WT;
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
			       TT_S2_ATTR_MEMATTR_OUTER_WT | \
			       TT_S2_ATTR_MEMATTR_NORMAL_INNER_WT;
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

static int mmu_map_level2_pages(unsigned long *tbase, unsigned long vbase,
		unsigned long pbase, size_t size, int type)
{
	int i;
	uint64_t attr;
	uint32_t offset;
	unsigned long tmp;

	attr = get_tt_description(type, LEVEL2_DESCRIPTION_TYPE);
	tmp = ALIGN(vbase, LEVEL1_ENTRY_MAP_SIZE);
	offset = (vbase - tmp) >> LEVEL2_OFFSET;

	for (i = 0; i < (size >> LEVEL2_OFFSET); i++) {
		if (*(tbase + offset) != 0)
			continue;

		*(tbase + offset) = (attr | \
			(pbase >> LEVEL2_OFFSET) << LEVEL2_OFFSET);
		vbase += LEVEL2_ENTRY_MAP_SIZE;
		pbase += LEVEL2_ENTRY_MAP_SIZE;
		offset++;
	}

	return 0;
}

static unsigned long alloc_page_tabe(void)
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

int mmu_map_mem(unsigned long t_base, unsigned long base,
		unsigned long vir_base, size_t size, int type)
{
	int i;
	unsigned long tmp;
	uint32_t offset;
	uint64_t value;
	uint64_t attr;
	size_t map_size;
	unsigned long *tbase = (unsigned long *)t_base;

	base = ALIGN(base, LEVEL2_ENTRY_MAP_SIZE);
	tmp = BALIGN(base + size, LEVEL2_ENTRY_MAP_SIZE);
	size = tmp - base;

	attr = get_tt_description(type, LEVEL1_DESCRIPTION_TYPE);

	while (size > 0) {
		offset = base >> LEVEL1_OFFSET;
		value = *(tbase + offset);
		if (value == 0) {
			tmp = (unsigned long)mmu_alloc_level2_mem();
			if (!tmp)
				return -ENOMEM;
			memset((char *)tmp, 0, LEVEL2_PAGES * SIZE_4K);

			*(tbase + offset) = attr | \
				((tmp >> LEVEL2_OFFSET) << LEVEL2_OFFSET);
			value = (uint64_t)tmp;
		} else {
			/* get the base address of the entry */
			value = value & 0x0000ffffffffffff;
			value = value >> LEVEL2_OFFSET;
			value = value << LEVEL2_OFFSET;
		}

		if (size > (LEVEL1_ENTRY_MAP_SIZE)) {
			map_size = BALIGN(base, LEVEL1_ENTRY_MAP_SIZE) - base;
			map_size = map_size ? map_size : LEVEL1_ENTRY_MAP_SIZE;
		} else {
			map_size = size;
		}

		mmu_map_level2_pages((unsigned long *)value, base,
				base, map_size, type);
		base += map_size;
		size -= map_size;
	}

	return 0;
}

static uint64_t generate_vtcr_el2(void)
{
	uint64_t value = 0;

	value |= (0x20 << 0);	// t0sz = 0x20 32bits vaddr
	value |= (0x01 << 6);	// SL0: 64kb/16 start at level2 4k start at level1
	value |= (0x0 << 8);	// Normal memory, Inner Non-cacheable
	value |= (0x0 << 10);	// Normal memory, Outer Non-cacheable
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

int el2_stage2_vmsa_init(void)
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
	.map_memory = mmu_map_mem,
	.alloc_page_table = alloc_page_tabe,
};

static int vmsa_module_init(struct vmm_module *module)
{
	module->context_size = sizeof(struct vmsa_context);
	module->pdata = (void *)&vmsa_mmu;
	module->state_init = vmsa_state_init;
	module->state_save = vmsa_state_save;
	module->state_restore = vmsa_state_restore;

	return 0;
}

VMM_MODULE_DECLARE(vmsa, "armv8-mmu",
	VMM_MODULE_NAME_MMU, (void *)vmsa_module_init);
