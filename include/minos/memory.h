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

/*
 * pstart - if this area is mapped as continous the pstart
 * is the phsical address of this vmm_area
 */
struct vmm_area {
	unsigned long start;
	unsigned long end;
	unsigned long pstart;
	size_t size;
	unsigned long flags;
	int vmid;			/* 0 - for self other for VM */
	struct list_head list;
	union {
		struct page *p_head;
		struct list_head b_head;
	};
};

struct mm_struct {
	/* the base address of the page table for the vm */
	unsigned long pgd_base;
	spinlock_t mm_lock;

	/*
	 * head - all page allocated to this VM, the mmu
	 * mapping table pages
	 *
	 * block_list - all the blocks allocated to this
	 * vm
	 */
	struct page *page_head;

	/*
	 * vmm_area_free : list to all the free vmm_area
	 * vmm_area_used : list to all the used vmm_area
	 * lock		 : spin lock for vmm_area allocate
	 */
	spinlock_t vmm_area_lock;
	struct list_head vmm_area_free;
	struct list_head vmm_area_used;

	void *vm;
};

int add_memory_region(uint64_t base, uint64_t size, uint32_t flags);
int split_memory_region(vir_addr_t base, size_t size, uint32_t flags);
int memory_region_type(struct memory_region *region);

#endif
