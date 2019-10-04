#ifndef _MINOS_MM_H_
#define _MINOS_MM_H_

#include <minos/list.h>
#include <minos/spinlock.h>
#include <minos/memattr.h>
#include <minos/bootmem.h>

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

struct memory_region {
	uint32_t flags;
	phy_addr_t phy_base;
	vir_addr_t vir_base;
	size_t size;
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

int add_memory_region(uint64_t base, uint64_t size, uint32_t flags);
int split_memory_region(vir_addr_t base, size_t size, uint32_t flags);
int memory_region_type(struct memory_region *region);

int mm_init(void);
void *malloc(size_t size);
void *zalloc(size_t size);
void free(void *addr);
void free_pages(void *addr);
void *__get_free_pages(int pages, int align);
struct page *__alloc_pages(int pages, int align);
void release_pages(struct page *page);
struct page *addr_to_page(void *addr);
void *__get_io_pages(int pages, int align);

#define page_to_addr(page)	(void *)(page->phy_base & __PAGE_MASK)

static inline void *get_free_page(void)
{
	return __get_free_pages(1, 1);
}

static inline void *get_free_pages(int pages)
{
	return __get_free_pages(pages, 1);
}

static inline struct page *alloc_pages(int pages)
{
	return __alloc_pages(pages, 1);
}

static inline struct page *alloc_page(void)
{
	return __alloc_pages(1, 1);
}

static inline void *get_io_page(void)
{
	return __get_io_pages(1, 1);
}

static inline void *get_io_pages(int pages)
{
	return __get_io_pages(pages, 1);
}

#ifdef CONFIG_SIMPLE_MM_ALLOCATER
static inline struct mem_block *alloc_mem_block(unsigned long flags)
{
	return NULL;
}

static inline void release_mem_block(struct mem_block *block)
{

}

static inline void add_slab_mem(unsigned long base, size_t size)
{

}

#else
struct mem_block *alloc_mem_block(unsigned long flags);
void release_mem_block(struct mem_block *block);
int has_enough_memory(size_t size);
void add_slab_mem(unsigned long base, size_t size);
#endif

#endif
