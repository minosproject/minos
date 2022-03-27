#ifndef _MINOS_MM_H_
#define _MINOS_MM_H_

#include <minos/list.h>
#include <minos/spinlock.h>
#include <minos/memattr.h>
#include <minos/memory.h>

#define pfn2phy(pfn) ((unsigned long)(pfn) << PAGE_SHIFT)
#define phy2pfn(addr) ((unsigned long)(addr) >> PAGE_SHIFT)

void *malloc(size_t size);
void *zalloc(size_t size);
void free(void *addr);
void free_pages(void *addr);
void *__get_free_pages(int pages, int align);

static inline void *get_free_page(void)
{
	return __get_free_pages(1, 1);
}

static inline void *get_free_pages(int pages)
{
	return __get_free_pages(pages, 1);
}

int create_host_mapping(unsigned long vir, unsigned long phy,
		size_t size, unsigned long flags);

int destroy_host_mapping(unsigned long vir, size_t size);

int change_host_mapping(unsigned long vir, unsigned long phy,
		unsigned long new_flags);

void *io_remap(virt_addr_t vir, size_t size);

int io_unmap(virt_addr_t vir, size_t size);

#endif
