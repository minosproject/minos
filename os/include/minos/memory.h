#ifndef __MINOS_MEMORY_H__
#define __MINOS_MEMORY_H__

#include <minos/types.h>
#include <minos/list.h>

#define MEMORY_REGION_F_NORMAL	(1 << 0)
#define MEMORY_REGION_F_DMA	(1 << 1)
#define MEMORY_REGION_F_RSV	(1 << 2)
#define MEMORY_REGION_F_VM	(1 << 3)
#define MEMORY_REGION_F_DTB	(1 << 4)
#define MEMORY_REGION_F_KERNEL	(1 << 5)
#define MEMORY_REGION_TYPE_MASK (0xff)

#define MEMORY_REGION_TYPE_NORMAL	0
#define MEMORY_REGION_TYPE_DMA		1
#define MEMORY_REGION_TYPE_RSV		2
#define MEMORY_REGION_TYPE_VM		3
#define MEMORY_REGION_TYPE_DTB		4
#define MEMORY_REGION_TYPE_KERNEL	5
#define MEMORY_REGION_TYPE_MAX		6

#define VM_MAX_MEM_REGIONS	10

struct memory_region {
	uint32_t flags;
	phy_addr_t phy_base;
	vir_addr_t vir_base;
	size_t size;
	size_t free_size;
	struct list_head list;
};

struct mem_block {
	unsigned long phy_base;
	uint16_t flags;
	uint16_t free_pages;
	uint16_t nr_pages;
	uint16_t bm_current;
	struct list_head list;
	unsigned long *pages_bitmap;
};

/*
 * phy_base[0:11] is the count of the continous page
 * phy_base[12:64] is the physical address of the page
 *
 * when get the physical address of the page, do
 * use page_to_addr(page) or addr_to_page() to convert
 * the address to page
 */
struct page {
	union {
		unsigned long phy_base;
		unsigned long size;
	};
	union {
		struct page *next;
		unsigned long magic;
	};
};

struct mm_struct {
	/* the base address of the page table for the vm */
	unsigned long pgd_base;

	/* the base address mapped in VM0 */
	unsigned long hvm_mmap_base;

	/*
	 * for the shared memory of native vm
	 * or the iomem space of guest vm
	 */
	union {
		unsigned long gvm_iomem_base;
		unsigned long shmem_base;
	};
	union {
		unsigned long gvm_iomem_size;
		unsigned long shmem_size;
	};

	/* for virtio devices */
	unsigned long virtio_mmio_gbase;
	void *virtio_mmio_iomem;
	size_t virtio_mmio_size;

	/*
	 * head - all page allocated to this VM, the mmu
	 * mapping table pages
	 *
	 * block_list - all the blocks allocated to this
	 * vm
	 */
	struct page *head;
	struct list_head block_list;

	/*
	 * list all the memory region for this VM, usually
	 * native VM may have at least one memory region, but
	 * guest VM will only have one region
	 */
	int nr_mem_regions;
	struct memory_region memory_regions[VM_MAX_MEM_REGIONS];

	spinlock_t lock;
};

int add_memory_region(uint64_t base, uint64_t size, uint32_t flags);
int split_memory_region(vir_addr_t base, size_t size, uint32_t flags);
int memory_region_type(struct memory_region *region);

#endif
