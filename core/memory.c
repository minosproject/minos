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

#include <minos/minos.h>
#include <minos/mm.h>
#include <minos/memory.h>
#ifdef CONFIG_DEVICE_TREE
#include <libfdt/libfdt.h>
#endif

#ifdef CONFIG_DEVICE_TREE
extern int of_mm_init(void);
#endif

struct slab_header {
	unsigned long size;
	union {
		unsigned long magic;
		struct slab_header *next;
	};
} __packed;

struct slab_type {
	uint32_t size;
	struct list_head list;
	struct slab_header *head;
};

#define PAGE_FLAGS_SHMEM (1 << 0)

struct page {
	uint32_t pfn;
	uint8_t flags;
	uint8_t align;
	uint16_t cnt;
	struct page *next;
} __packed;

#define HASH_TABLE_SIZE	8

#define SLAB_MEM_BASE (511UL * 1024 * 1024 * 1024)
#define SLAB_MEM_SIZE (128UL * 1024 * 1024)
#define SLAB_MEM_END (SLAB_MEM_BASE + SLAB_MEM_SIZE)

#define SLAB_MIN_DATA_SIZE		(16)
#define SLAB_MIN_DATA_SIZE_SHIFT	(4)
#define SLAB_HEADER_SIZE		sizeof(struct slab_header)
#define SLAB_MIN_SIZE			(SLAB_MIN_DATA_SIZE + SLAB_HEADER_SIZE)
#define SLAB_MAGIC			(0xdeadbeef)

/*
 * will try to get hugepage when first time once
 * system bootup.
 */
static DEFINE_SPIN_LOCK(mm_lock);
static void *slab_base;
static void *page_base;
static struct page *free_page_head;
static struct page *used_page_head;
static struct list_head slab_hash_table[HASH_TABLE_SIZE];

#define hash_id(size) (((size) >> SLAB_MIN_DATA_SIZE_SHIFT) % HASH_TABLE_SIZE)

static size_t inline get_slab_alloc_size(size_t size)
{
	return BALIGN(size, SLAB_MIN_DATA_SIZE);
}

static void *malloc_from_hash_table(size_t size)
{
	int id = hash_id(size);
	struct slab_type *st;
	struct slab_header *sh;

	/*
	 * find the related slab mem id and try to fetch
	 * a free slab memory from the hash cache.
	 */
	list_for_each_entry(st, &slab_hash_table[id], list) {
		if (st->size != size)
			continue;

		if (st->head == NULL)
			return NULL;

		sh = st->head;
		st->head = sh->next;
		sh->magic = SLAB_MAGIC;

		return ((void *)sh + SLAB_HEADER_SIZE);
	}

	return NULL;
}

static void *malloc_from_slab_heap(size_t size)
{
	unsigned long slab_size;
	struct slab_header *sh;

	if (ULONG(slab_base) >= ULONG(page_base)) {
		pr_err("no more memory for slab\n");
		return NULL;
	}

	slab_size = ULONG(page_base) - ULONG(slab_base);
	size += SLAB_HEADER_SIZE;
	if (slab_size < size) {
		pr_err("no enough memory for slab 0x%x 0x%x\n",
				size, slab_size);
		return NULL;
	}

	sh = (struct slab_header *)slab_base;
	sh->magic = SLAB_MAGIC;
	sh->size = size - SLAB_HEADER_SIZE;

	slab_base += size;

	return ((void *)sh + SLAB_HEADER_SIZE);
}

static void free_slab(void *addr)
{
	struct slab_header *header;
	struct slab_type *st;
	int id;

	ASSERT(ULONG(addr) < ULONG(slab_base));
	header = (struct slab_header *)((unsigned long)addr -
			SLAB_HEADER_SIZE);
	ASSERT(header->magic == SLAB_MAGIC);
	id = hash_id(header->size);

	list_for_each_entry(st, &slab_hash_table[id], list) {
		if (st->size != header->size)
			continue;

		header->next = st->head;
		st->head = header;
		return;
	}

	/*
	 * create new slab type and add the new slab header
	 * to the slab cache.
	 */
	st = malloc_from_slab_heap(sizeof(struct slab_type));
	if (!st) {
		pr_err("alloc memory for slab type failed\n");
		return;
	}

	st->size = header->size;
	st->head = NULL;
	list_add_tail(&slab_hash_table[id], &st->list);

	header->next = st->head;
	st->head = header;
}

static void *__malloc(size_t size)
{
	void *mem;

	mem = malloc_from_hash_table(size);
	if (mem != NULL)
		return mem;

	return malloc_from_slab_heap(size);
}

void *malloc(size_t size)
{
	void *mem;

	ASSERT(size != 0);
	size = get_slab_alloc_size(size);

	spin_lock(&mm_lock);
	mem =  __malloc(size);
	spin_unlock(&mm_lock);

	if (!mem) {
		pr_err("malloc fail for 0x%x\n");
		dump_stack(NULL, NULL);
	}

	return mem;
}

void *zalloc(size_t size)
{
	void *addr = malloc(size);
	if (addr)
		memset(addr, 0, size);
	return addr;
}

void free(void *addr)
{
	ASSERT(addr != NULL);
	spin_lock(&mm_lock);
	free_slab(addr);
	spin_unlock(&mm_lock);
}

static inline void add_used_page(struct page *page)
{
	page->next = used_page_head;
	used_page_head = page;
}

static struct page *find_used_page(void *addr)
{
	struct page *head = used_page_head;
	struct page *prev = NULL;

	while (head) {
		if (head->pfn == (unsigned long)addr >> PAGE_SHIFT) {
			if (prev)
				prev->next = head->next;
			else
				used_page_head = NULL;
			head->next = NULL;

			return head;
		}

		prev = head;
		head = head->next;
	}

	return NULL;
}

static inline void __free_page(struct page *page)
{
	page->next = free_page_head;
	free_page_head = page;
}

void free_pages(void *addr)
{
	struct page *page;

	ASSERT(IS_PAGE_ALIGN(addr) || (addr != NULL));
	spin_lock(&mm_lock);
	page = find_used_page(addr);
	if (page)
		__free_page(page);
	else
		pr_err("%s: free wrong page 0x%x\n", __func__, addr);
	spin_unlock(&mm_lock);
}

static struct page *alloc_new_pages(int pages, unsigned long align)
{
	unsigned long tmp = (unsigned long)page_base;
	struct page *recycle = NULL;
	unsigned long base, rbase;
	struct page *page;

	base = tmp - pages * PAGE_SIZE;
	base = ALIGN(base, align);
	if (base < (unsigned long)slab_base) {
		pr_err("no more pages %d 0x%x\n", pages, align);
		return NULL;
	}

	rbase = base + pages * PAGE_SIZE;
	if (rbase != tmp) {
		recycle = __malloc(sizeof(struct page));
		if (!recycle) {
			pr_err("can not allocate memory for page\n");
			return NULL;
		}

		recycle->pfn = rbase >> PAGE_SHIFT;
		recycle->flags = 0;
		recycle->align = 1;
		recycle->cnt = (tmp - rbase) >> PAGE_SHIFT;
		recycle->next = NULL;
		__free_page(recycle);
	}

	page = __malloc(sizeof(struct page));
	if (!page) {
		pr_err("can not allocate memory for page\n");
		if (recycle)
			free_slab(recycle);
		return NULL;
	}

	page->pfn = base >> PAGE_SHIFT;
	page->flags = 0;
	page->cnt = pages;
	page->align = align >> PAGE_SHIFT;
	page->next = NULL;

	page_base = (void *)base;

	return page;
}

static struct page *__alloc_pages(int pages, int align)
{
	struct page *page = NULL;
	struct page *tmp, *prev = NULL;

	switch (align) {
	case 1:
	case 2:
	case 4:
	case 8:
		break;
	default:
		pr_err("%s:unsupport align value %d\n", __func__, align);
		return NULL;
	}

	spin_lock(&mm_lock);
	tmp = free_page_head;

	/*
	 * try to get the free page from the free list.
	 */
	while (tmp) {
		if ((tmp->cnt == pages) && (tmp->align == align)) {
			page = tmp;
			break;
		}

		prev = tmp;
		tmp = tmp->next;
	}

	if (!page) {
		page = alloc_new_pages(pages, PAGE_SIZE * align);
	} else {
		if (prev != NULL) {
			prev->next = page->next;
			page->next = NULL;
		} else {
			free_page_head = NULL;
		}
	}

	add_used_page(page);
	spin_unlock(&mm_lock);

	return page;
}

void *__get_free_pages(int pages, int align)
{
	struct page *page = NULL;

	page = __alloc_pages(pages, align);
	if (!page)
		return NULL;

	return (void *)ptov(pfn2phy(page->pfn));
}

static void slab_init(void)
{
	int i;

	pr_notice("slab memory allocator init ...\n");
	for (i = 0; i < HASH_TABLE_SIZE; i++)
		init_list(&slab_hash_table[i]);
}

static void memory_init(void)
{
	slab_base = (void *)BALIGN(ptov(minos_end), 16);
	page_base = (void *)ptov(minos_start + CONFIG_MINOS_RAM_SIZE);
	pr_notice("MEM slab 0x%x page 0x%x\n",
			(unsigned long)slab_base, (unsigned long)page_base);
	ASSERT(page_base > slab_base);
}

static void memory_region_init(void)
{
	unsigned long end = minos_start + CONFIG_MINOS_RAM_SIZE;
	extern void set_ramdisk_address(void *start, void *end);
	struct memory_region *re;

#ifdef CONFIG_DEVICE_TREE
	of_mm_init();
#endif
	/*
	 * check the memory region configruation, each region should
	 * not overlap with host memory region.
	 */
	list_for_each_entry(re, &mem_list, list) {
		if (re->type == MEMORY_REGION_TYPE_KERNEL) {
			if ((re->phy_base >= minos_start) &&
					((re->phy_base + re->size) < end)) {
				pr_err("invalid memory region [0x%x ---> 0x%x]",
						re->phy_base, re->phy_base + re->size);
				BUG();
			}
		}

		if (re->type == MEMORY_REGION_TYPE_RAMDISK) {
			pr_notice("set ramdisk address 0x%lx 0x%lx\n",
					re->phy_base, re->size);
			set_ramdisk_address((void *)re->phy_base,
					(void *)(re->phy_base + re->size));
		}
	}
}

void mm_init(void)
{
	memory_init();
	slab_init();
	memory_region_init();
}
