#ifndef _MINOS_MM_H_
#define _MINOS_MM_H_

#include <minos/list.h>
#include <minos/spinlock.h>
#include <minos/memattr.h>
#include <minos/bootmem.h>
#include <minos/memory.h>

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
