#ifndef _MVISOR_MMU_H_
#define _MVISOR_MMU_H_

#include <asm/asm_mmu.h>

#define DESCRIPTION_TABLE	(0x0)
#define DESCRIPTION_BLOCK	(0x1)
#define DESCRIPTION_PAGE	(0x2)

#define MEM_TYPE_NORMAL		(0x0)
#define MEM_TYPE_IO		(0x1)

#define MEM_REGION_NAME_SIZE	32

struct memory_region {
	phy_addr_t mem_base;
	size_t size;
	int type;
	char name[MEM_REGION_NAME_SIZE];
	struct list_head list;
};

int mmu_map_memory_region_list(phy_addr_t tbase,
		struct list_head *mem_list);

phy_addr_t mmu_map_vm_memory(struct list_head *mem_list);

uint64_t mmu_generate_vtcr_el2(void);

uint64_t mmu_get_vttbr_el2_base(uint32_t vmid, phy_addr_t base);

#endif
