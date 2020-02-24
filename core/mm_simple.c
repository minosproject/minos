/*
 * Copyright (C) 2019 Min Le (lemin9538@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <minos/minos.h>
#include <minos/mm.h>
#include <minos/bootmem.h>

struct mem_zone {
	void *mem_base;
	void *free_base;
	size_t free_size;
	size_t size;
	spinlock_t lock;
};

#define MAX_MEM_ZONE	10
static struct mem_zone mem_zones[MAX_MEM_ZONE];
static int nr_mem_zone;
static struct page *root_page;
static DEFINE_SPIN_LOCK(mem_lock);

extern void *bootmem_page_base;
extern void *bootmem_start;
extern unsigned long bootmem_size;
extern struct list_head mem_list;

#define MEM_TYPE_SLAB	0xabcdef01
#define MEM_TYPE_PAGE	0xabcdef02

static void init_mem_zone(unsigned long base, size_t size)
{
	struct mem_zone *zone;

	if (base == CONFIG_MINOS_START_ADDRESS) {
		zone = &mem_zones[0];
		zone->mem_base = (void *)base;
		zone->free_base = bootmem_start;
		zone->free_size = bootmem_size;
		zone->size = CONFIG_MINOS_RAM_SIZE;
	} else {
		zone = &mem_zones[nr_mem_zone];
		zone->mem_base = (void *)base;
		zone->free_base = (void *)base;
		zone->free_size = size;
		nr_mem_zone++;
	}
}

static void *__malloc_from_zone(struct mem_zone *zone, int size)
{
	void *addr = NULL;

	spin_lock(&zone->lock);
	if (zone->free_size < size)
		goto out;
	
	addr = zone->free_base;
	zone->free_size -= size;
	zone->free_base += size;

out:
	spin_unlock(&zone->lock);
	return addr;
}

static void *__malloc(int size)
{
	int i;
	struct mem_zone *zone;
	void *addr;

	for (i = 0; i < nr_mem_zone; i++) {
		zone = &mem_zones[i];
		addr = __malloc_from_zone(zone, size);
		if (addr)
			return addr;
	}

	return NULL;
}

static inline void detach_page(struct page *prev, struct page *page)
{
	if (page == root_page) {
		root_page = page->next;
	} else {
		if (prev == NULL)
			panic("page was root page\n");

		prev->next = page->next;
	}
}

struct page *malloc_from_free_list(int size)
{
	unsigned long flags;
	struct page *page = NULL;
	struct page *min_page = root_page;
	struct page *tmp = root_page;
	struct page *prev = NULL;

	spin_lock_irqsave(&mem_lock, flags);

	do {
		/*
		 * if the size is equal the size which the task
		 * request, return directly, otherwise search for
		 * the minimal size that can match the request
		 */
		if (tmp->size == size) {
			detach_page(prev, tmp);
			page = tmp;
			goto out;
		}

		if (min_page->size < tmp->size)
			min_page = tmp;

		tmp = tmp->next;
		prev = tmp;
	} while (1);

out:
	spin_unlock_irqrestore(&mem_lock, flags);

	return page;
}

static struct page *_malloc(int size)
{
	struct page *page;

	size = BALIGN(size, sizeof(unsigned long));

	page = malloc_from_free_list(size);
	if (page)
		goto out;

	page = __malloc(sizeof(*page) + size);
	if (!page)
		return NULL;

	page->size = size;
	page->magic = MEM_TYPE_SLAB;

out:
	return (void *)((void *)page + sizeof(*page));
}

void *malloc(size_t size)
{
	struct page *page;

	page = _malloc(size);
	if (page)
		return NULL;

	return (void *)((void *)page + sizeof(*page));
}

void *zalloc(size_t size)
{
	void *addr = malloc(size);

	if (addr)
		memset(addr, 0, size);

	return addr;
}

void *__get_free_pages(int pages, int align)
{
	return malloc(pages * PAGE_SIZE);
}

struct page *__alloc_pages(int pages, int algin)
{
	return _malloc(pages * PAGE_SIZE);
}

struct page *addr_to_page(void *addr)
{
	return (struct page *)(addr - sizeof(struct page));
}

void *__get_io_pages(int pages, int align)
{
	return malloc(pages * PAGE_SIZE);
}

void __free(struct page *page)
{
	unsigned long flags;

	spin_lock_irqsave(&mem_lock, flags);
	page->next = root_page;
	root_page = page;
	spin_unlock_irqrestore(&mem_lock, flags);
}

void free(void *addr)
{
	struct page *page;

	if (addr == NULL)
		return;

	page = addr - sizeof(*page);
	if (page->magic != MEM_TYPE_SLAB) {
		pr_err("mem is not alloced by malloc() 0x%x\n", addr);
		return;
	}

	__free(page);
}

void release_pages(struct page *page)
{
	if (page)
		__free(page);
}

int mm_do_init(void)
{
	struct memory_region *region;

	/* 
	 * this memory allocator is design for mcu system
	 * which does not include a MMU
	 */
	pr_info("simple memory allocator init...\n");

	/* at least one memory zoen for minos */
	nr_mem_zone = 1;

	list_for_each_entry(region, &mem_list, list)
		init_mem_zone(region->vir_base, region->size);

	return 0;
}
