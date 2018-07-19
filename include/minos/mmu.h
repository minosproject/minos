#ifndef _MINOS_MMU_H_
#define _MINOS_MMU_H_

#include <minos/types.h>
#include <asm/asm_mmu.h>

#define DESCRIPTION_TABLE	(0x0)
#define DESCRIPTION_BLOCK	(0x1)
#define DESCRIPTION_PAGE	(0x2)

#define MEM_TYPE_NORMAL		(0x0)
#define MEM_TYPE_IO		(0x1)

#define MEM_REGION_NAME_SIZE	32

struct vm;

struct mmu_chip {
	int (*map_guest_memory)(struct vm *vm,
			unsigned long page_table_base,
			unsigned long phy_base,
			unsigned long vir_base,
			size_t size, int type);

	int (*map_host_memory)(unsigned long vir,
			unsigned long phy,
			size_t size, int type);

	unsigned long (*alloc_guest_pt)(void);
};

unsigned long alloc_guest_page_table(void);
int map_guest_memory(struct vm *, unsigned long, unsigned long,
		unsigned long, size_t, int);

int map_host_memory(unsigned long vir,
		unsigned long phy, size_t size, int type);

int io_remap(unsigned long vir, unsigned long phy, size_t size);
int mmu_init(void);

#endif
