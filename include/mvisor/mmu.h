#ifndef _MVISOR_MMU_H_
#define _MVISOR_MMU_H_

#include <mvisor/types.h>
#include <asm/asm_mmu.h>

#define DESCRIPTION_TABLE	(0x0)
#define DESCRIPTION_BLOCK	(0x1)
#define DESCRIPTION_PAGE	(0x2)

#define MEM_TYPE_NORMAL		(0x0)
#define MEM_TYPE_IO		(0x1)

#define MEM_REGION_NAME_SIZE	32

struct mmu_chip {
	int (*map_memory)(phy_addr_t page_table_base, phy_addr_t phy_base,
			phy_addr_t vir_base, size_t size, int type);
	phy_addr_t (*alloc_page_table)(void);
};

phy_addr_t mmu_alloc_page_table(void);

int mmu_map_memory(phy_addr_t page_table_base, phy_addr_t phy_base,
		phy_addr_t vir_base, size_t size, int type);

int vmm_mmu_init(void);

#endif
