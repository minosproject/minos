#ifndef _MINOS_MM_H_
#define _MINOS_MM_H_

#include <minos/list.h>
#include <minos/spinlock.h>
#include <minos/memattr.h>

struct memtag;
struct vm;

struct mem_block {
	unsigned long phy_base;
	uint16_t vmid;
	uint16_t flags;
	uint16_t free_pages;
	uint16_t bm_current;
	struct list_head list;
	unsigned long *pages_bitmap;
} __packed__;

/*
 * phy_base[0:11] is the count of the continous page
 * phy_base[12:64] is the physical address of the page
 *
 * when get the physical address of the page, do
 * use page_to_addr(page) or addr_to_page() to convert
 * the address to page
 */
struct page {
	unsigned long phy_base;
	struct page *next;
} __packed__;

struct memory_region {
	int type;
	uint32_t vmid;
	unsigned long mem_base;
	unsigned long vir_base;
	size_t size;
	struct list_head list;
};

extern struct list_head mem_list;

int mm_init(void);
void *malloc(size_t size);
void *zalloc(size_t size);
void free(void *addr);
void free_pages(void *addr);
void *__get_free_pages(int pages, int align);
struct page *__alloc_pages(int pages, int align);
void release_pages(struct page *page);

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

struct mem_block *alloc_mem_block(unsigned long flags);
void release_mem_block(struct mem_block *block);
int has_enough_memory(size_t size);

#endif
