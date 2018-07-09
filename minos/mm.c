/*
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
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

#include <minos/types.h>
#include <config/config.h>
#include <minos/spinlock.h>
#include <minos/minos.h>
#include <minos/init.h>
#include <minos/mmu.h>

extern unsigned char __code_start;
extern unsigned char __code_end;

#ifdef CONFIG_MINOS_MM_SIMPLE_MODE

static char *free_mem_base = NULL;
static unsigned long free_mem_size = 0;
static spinlock_t mem_block_lock;
static char *free_4k_base = 0;

void mm_init(void)
{
	size_t size;

	spin_lock_init(&mem_block_lock);
	size = (&__code_end) - (&__code_start);
	size = BALIGN(size, sizeof(unsigned long));
	free_mem_base = (char *)(CONFIG_MINOS_START_ADDRESS + size);
	free_mem_size = CONFIG_MINOS_RAM_SIZE - size;

	/*
	 * assume the memory region is 4k align
	 */
	free_4k_base = free_mem_base + free_mem_size;
}

void *malloc(size_t size)
{
	size_t request_size;
	char *base;

	request_size = BALIGN(size, sizeof(unsigned long));

	spin_lock(&mem_block_lock);

	if (free_mem_size < request_size)
		base = NULL;
	else
		base = free_mem_base;

	free_mem_base += request_size;
	free_mem_size -= request_size;

	spin_unlock(&mem_block_lock);

	return base;
}

void *zalloc(size_t size)
{
	char *base;

	base = malloc(size);
	if (!base)
		return NULL;

	memset(base, 0, size);
	return base;
}

void *get_free_pages(int pages)
{
	size_t request_size = pages * SIZE_4K;
	char *base;

	if (pages <= 0)
		return NULL;

	spin_lock(&mem_block_lock);
	if (free_mem_size < request_size)
		return NULL;

	if (((unsigned long)free_4k_base - request_size) < free_mem_size)
		return NULL;

	base = free_4k_base - request_size;
	free_4k_base = base;
	free_mem_size -= request_size;
	spin_unlock(&mem_block_lock);

	return base;
}

void free(void *addr)
{
	panic("free not support on MM_SIMPLE mode\n");
}

void free_pages(void *addr)
{
	panic("free_pages not supported on MM_SIMPLE mode\n");
}

#else

#define MM_MAX_SECTION		(10)
#define CONFIG_PAGE_SIZE	(SIZE_4K)
#define CONFIG_PAGE_SHIFT	(12)
#define PAGE_SIZE		(CONFIG_PAGE_SIZE)
#define PAGE_SHIFT		(CONFIG_PAGE_SHIFT)

struct mm_section {
	unsigned long phy_start;
	size_t size;
	size_t nr_pages;
	size_t free_pages;
	uint32_t *bitmap;
	uint32_t bm_end;
	uint32_t bm_current;
	struct page *page_table;
	char name[32];
};

struct mm_pool {
	struct mm_section sections[MM_MAX_SECTION];
	int nr_sections;
	spinlock_t lock;
	uint32_t page_size;
	size_t nr_pages;
	size_t free_pages;
};

struct page {
	unsigned long phy_address;
	struct list_head plist;
	uint64_t count : 16;
	uint64_t pinfo : 16;
	uint64_t vmid : 16;
};

static struct mm_pool  __mm_pool;
static struct mm_pool *mm_pool = &__mm_pool;

static int add_memory_section(unsigned long mem_base,
		unsigned long mem_end, char *name)
{
	struct mm_section *ms;
	size_t size;

	spin_lock(&mm_pool->lock);

	if (mm_pool->nr_sections >= MM_MAX_SECTION)
		return -EINVAL;

	ms = &mm_pool->sections[mm_pool->nr_sections];
	mem_base = BALIGN(mem_base, PAGE_SIZE);
	size = mem_end - mem_base;
	size = ALIGN(size, PAGE_SIZE);

	if (size == 0)
		return -EINVAL;

	pr_info("add memory region start:0x%x size:0x%x\n", mem_base, size);
	ms->phy_start = mem_base;
	ms->size = size;
	ms->nr_pages = size >> PAGE_SHIFT;
	ms->free_pages = ms->nr_pages;
	strncpy(ms->name, name, strlen(name) > 31 ? 31 : strlen(name));

	mm_pool->free_pages += ms->free_pages;
	mm_pool->nr_pages += ms->nr_pages;

	mm_pool->nr_sections++;

	spin_unlock(&mm_pool->lock);

	return 0;
}

static void parse_system_regions(void)
{
	int i;
	struct memory_region *regions;
	struct memory_region *region;

	regions = (struct memory_region *)get_memory_regions();
	if (regions = NULL)
		panic("No memory config for system\n");

	for (i = 0; i < MM_MAX_SECTION; i++) {
		region = &regions[i];
		if (region->mem_end == 0)
			break;

		add_memory_section(region->mem_base,
				region->mem_end, region->name);
	}
}

static unsigned long pages_bitmap_init(unsigned long base)
{
	int i;
	struct mm_section *section;
	uint32_t *bitmap_base = (uint32_t *)base;

	/*
	 * allocate memory for bitmap
	 */
	for (i = 0; i < mm_pool->nr_sections; i++) {
		section = &mm_pool->sections[i];
		section->bitmap = bitmap_base;
		section->bm_current = 0;
		section->bm_end = section->nr_pages;
		bitmap_base += bits_to_long(section->nr_pages);
	}

	memset((char *)base, 0, (unsigned long)bitmap_base - base);

	return (unsigned long)bitmap_base;
}

static unsigned long pages_table_init(unsigned long base)
{
	int i;
	struct mm_section *section;
	struct page *tmp = (struct page *)base;

	/*
	 * allocate memory for page table
	 */
	for (i = 0; i < mm_pool->nr_sections; i++) {
		section = &mm_pool->sections[i];
		section->page_table = tmp;
		tmp += section->nr_pages;
	}

	memset((char *)base, 0, (unsigned long)tmp - base);

	return (unsigned long)tmp;
}

static int mm_sections_init(void)
{
	int i;
	size_t boot_page_size;
	unsigned long mem_start, mem_end;
	struct mm_section *section = NULL;
	struct mm_section *boot_section = NULL;
	unsigned long code_base = (unsigned long)&__code_start;
	struct page *page;

	spin_lock(&mm_pool->lock);

	mem_start = (unsigned long)&__code_end;
	mem_start = BALIGN(mem_start, sizeof(unsigned long));
	mem_start = pages_table_init(mem_start);

	mem_start = BALIGN(mem_start, sizeof(unsigned long));
	mem_start = pages_bitmap_init(mem_start);

	for (i = 0; i < mm_pool->nr_sections; i++) {
		section = &mm_pool->sections[i];
		if ((code_base >= section->phy_start) &&
				(code_base < (section->phy_start + section->size))) {
			boot_section = section;
			break;
		}
	}

	if (!boot_section)
		panic("errors in memory config\n");

	mem_start = mem_start - code_base;
	mem_end = ALIGN(mem_start, PAGE_SIZE);
	mem_start = BALIGN(mem_start, PAGE_SIZE);

	boot_page_size = (mem_start - code_base) / PAGE_SIZE;
	page = section->page_table;
	for (i = 0; i < boot_page_size; i++) {
		set_bit(section->bitmap, i);

		page->phy_address = code_base;
		init_list(&page->plist);
		page->count = 1;
		page->vmid = 0xffff;

		code_base += PAGE_SIZE;
		page++;
	}

	/*
	 * put this page to the slab allocater do not
	 * waste the memory
	 */
	if ((mem_end - mem_start) > 512) {
		// TBD
	}

	spin_unlock(&mm_pool->lock);
}

static void update_memory_bitmap(uint32_t *bitmap,
		long index, int count, int update)
{
	long i;

	if (update) {
		for (i = index; i < index + count; i++)
			set_bit(bitmap, i);
	} else {
		for (i = index; i < index + count; i++)
			set_bit(bitmap, i);
	}
}

static int init_pages(struct mm_section *section, long index, int count)
{
	return 0;
}

static void *get_pages_from_section(struct mm_section *section, int count)
{
	long index;
	unsigned long ret;

	index = bitmap_find_free_base(section->bitmap,
			section->bm_current, 0,
			section->nr_pages, count);
	if (index < 0) {
		pr_error("Can not find %d continous pages\n", count);
		return NULL;
	}

	update_memory_bitmap(section->bitmap, index, count, 1);
	section->bm_current = index + count;
	if (section->bm_current >= section->bm_end)
		section->bm_current = 0;

	section->free_pages -= count;
	init_pages(section, index, count);

	ret = section->phy_start + (index << PAGE_SHIFT);

	return (void *)ret;
}

void *get_free_pages(int count)
{
	int i;
	void *base = NULL;
	struct mm_section *section = NULL;

	if (count <= 0) {
		pr_error("get_free_pages count count:%d\n", count);
		return NULL;
	}

	spin_lock(&mm_pool->lock);

	if (mm_pool->free_pages < count)
		return NULL;

	for (i = 0; i < mm_pool->nr_sections; i++) {
		section = &mm_pool->sections[i];
		if (section->free_pages >= count)
			break;
	}

	if (i == mm_pool->nr_sections)
		return NULL;

	base = get_pages_from_section(section, count);
	if (base)
		mm_pool->free_pages -= count;

	spin_unlock(&mm_pool->lock);

	return base;
}

char *malloc(size_t size)
{
	return NULL;
}

int mm_init(void)
{
	memset((char *)mm_pool, 0, sizeof(struct mm_pool));
	spin_lock_init(&mm_pool->lock);

	parse_system_regions();

	mm_sections_init();

	return 0;
}

#endif
