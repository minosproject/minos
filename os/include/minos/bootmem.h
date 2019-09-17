#ifndef __MINOS_BOOTMEM_H__
#define __MINOS_BOOTMEM_H__

#define MEMORY_REGION_F_RSV	(1 << 0)

struct memory_region {
	uint32_t flags;
	phy_addr_t phy_base;
	vir_addr_t vir_base;
	size_t size;
	struct list_head list;
};

void *alloc_boot_mem(size_t size);
void *alloc_boot_pages(int pages);
int add_memory_region(uint64_t base, uint64_t size, uint32_t flags);
int split_memory_region(vir_addr_t base, size_t size, uint32_t flags);

#endif
