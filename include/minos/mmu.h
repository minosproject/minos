#ifndef _MINOS_MMU_H_
#define _MINOS_MMU_H_

#include <minos/types.h>
#include <minos/memattr.h>
#include <asm/pagetable.h>

#define PGD_RANGE_OFFSET	(__PGD_RANGE_OFFSET)
#define PGD_DES_OFFSET		(__PGD_DES_OFFSET)
#define PGD_ENTRY_OFFSET_MASK	(__PGD_ENTRY_OFFSET_MASK)

#define PUD_RANGE_OFFSET	(__PUD_RANGE_OFFSET)
#define PUD_DES_OFFSET		(__PUD_DES_OFFSET)
#define PUD_ENTRY_OFFSET_MASK	(__PUD_ENTRY_OFFSET_MASK)

#define PMD_RANGE_OFFSET	(__PMD_RANGE_OFFSET)
#define PMD_DES_OFFSET		(__PMD_DES_OFFSET)
#define PMD_ENTRY_OFFSET_MASK	(__PMD_ENTRY_OFFSET_MASK)

#define PTE_RANGE_OFFSET	(__PTE_RANGE_OFFSET)
#define PTE_DES_OFFSET		(__PTE_DES_OFFSET)
#define PTE_ENTRY_OFFSET_MASK	(__PTE_ENTRY_OFFSET_MASK)

#define PGD_MAP_SIZE		(1UL << PGD_RANGE_OFFSET)
#define PUD_MAP_SIZE		(1UL << PUD_RANGE_OFFSET)
#define PMD_MAP_SIZE		(1UL << PMD_RANGE_OFFSET)
#define PTE_MAP_SIZE		(1UL << PTE_RANGE_OFFSET)

struct vm;

unsigned long alloc_guest_page_table(void);
int map_guest_memory(struct vm *, unsigned long, unsigned long,
		unsigned long, size_t, int);

int unmap_guest_memory(struct vm *vm, unsigned long tt,
		unsigned long vir, size_t size, int type);

int unmap_host_memory(unsigned long vir, size_t size, int type);

int map_host_memory(unsigned long vir,
		unsigned long phy, size_t size, int type);

int io_remap(unsigned long vir, unsigned long phy, size_t size);

#endif
