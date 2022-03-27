#ifndef __MINOS_MEMORY_H__
#define __MINOS_MEMORY_H__

#include <minos/types.h>
#include <minos/list.h>

enum {
	MEMORY_REGION_TYPE_NORMAL = 0,
	MEMORY_REGION_TYPE_DMA,
	MEMORY_REGION_TYPE_RSV,
	MEMORY_REGION_TYPE_VM,
	MEMORY_REGION_TYPE_DTB,
	MEMORY_REGION_TYPE_KERNEL,
	MEMORY_REGION_TYPE_RAMDISK,
	MEMORY_REGION_TYPE_MAX
};

#define MEMORY_REGION_F_NORMAL	(1 << MEMORY_REGION_TYPE_NORMAL)
#define MEMORY_REGION_F_DMA	(1 << MEMORY_REGION_TYPE_DMA)
#define MEMORY_REGION_F_RSV	(1 << MEMORY_REGION_TYPE_RSV)
#define MEMORY_REGION_F_VM	(1 << MEMORY_REGION_TYPE_VM)
#define MEMORY_REGION_F_DTB	(1 << MEMORY_REGION_TYPE_DTB)
#define MEMORY_REGION_F_KERNEL	(1 << MEMORY_REGION_TYPE_KERNEL)
#define MEMORY_REGION_F_RAMDISK	(1 << MEMORY_REGION_TYPE_RAMDISK)
#define MEMORY_REGION_F_MASK 	0xff

#define VM_MAX_MEM_REGIONS	10

extern unsigned long minos_start;
extern unsigned long minos_bootmem_base;
extern unsigned long minos_stack_top;
extern unsigned long minos_stack_bottom;
extern unsigned long minos_end;

extern struct list_head mem_list;

#define for_each_memory_region(region)	\
	list_for_each_entry(region, &mem_list, list)

struct memory_region {
	int flags;
	phy_addr_t phy_base;
	size_t size;
	struct list_head list;
};

int add_memory_region(uint64_t base, uint64_t size, uint32_t flags);
int split_memory_region(virt_addr_t base, size_t size, uint32_t flags);
int memory_region_type(struct memory_region *region);
void dump_memory_info(void);

#endif
