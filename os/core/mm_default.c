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
#include <minos/mm.h>
#include <minos/mmu.h>

extern unsigned char __code_start;
extern void *bootmem_end;
extern void *bootmem_start;
extern void *bootmem_page_base;
extern unsigned long bootmem_size;
extern struct list_head mem_list;

#define MEM_SECTION_NORMAL	MEMORY_REGION_TYPE_NORMAL
#define MEM_SECTION_DMA		MEMORY_REGION_TYPE_DMA
#define MEM_SECTION_PAGE	(MEMORY_REGION_TYPE_MAX)

#define MEM_SECTION_F_NORMAL	MEMORY_REGION_F_NORMAL
#define MEM_SECTION_F_DMA	MEMORY_REGION_F_DMA
#define MEM_SECTION_TYPE_MASK	(0xff)

#define MEM_SECTION_F_PAGE	(1 << 8)
#define MEM_SECTION_F_BLOCK	(1 << 9)

struct mem_section {
	unsigned long phy_base;
	int id;
	int type;
	size_t size;
	size_t nr_blocks;
	size_t free_blocks;
	unsigned long *bitmap;
	unsigned long bm_end;
	unsigned long bm_current;
	spinlock_t lock;
	union {
		struct mem_block *blocks;
		struct page *pages;
	};
};

struct slab_header {
	unsigned long size;
	union {
		unsigned long magic;
		struct slab_header *next;
	};
};

struct slab_pool {
	size_t size;
	struct slab_header *head;
	unsigned long nr;
};

struct slab {
	int pool_nr;
	int free_size;
	unsigned long slab_free;
	struct list_head block_list;
	struct slab_pool *pool;
	spinlock_t lock;
	size_t alloc_blocks;
};

struct page_pool {
	spinlock_t lock;
	uint32_t meta_blocks;
	uint32_t page_blocks;
	struct list_head meta_list;
	struct list_head block_list;
};

#define MIN_BLOCK_PAGE_SIZE	(8 * PAGE_SIZE)
#define PAGE_METAS_IN_BLOCK	(254)
#define BLOCK_BITMAP_SIZE	(64)
#define PAGE_METAS_BITMAP_SIZE	(DIV_ROUND_UP(PAGE_METAS_IN_BLOCK, BITS_PER_BYTE))
#define PAGE_META_SIZE		(BLOCK_BITMAP_SIZE + PAGES_IN_BLOCK * sizeof(struct page))

static int nr_sections;
struct mem_section mem_sections[MAX_MEM_SECTIONS];
static struct slab slab;
static struct slab *pslab = &slab;
static struct page_pool __page_pool;
static struct page_pool __io_pool;
static struct page_pool *io_pool = &__io_pool;
static struct page_pool *page_pool = &__page_pool;
static size_t free_blocks;

static void inline add_slab_to_slab_pool(struct slab_header *header,
		struct slab_pool *pool);

static struct slab_pool slab_pool[] = {
	{16, 	NULL, 0},
	{32, 	NULL, 0},
	{48, 	NULL, 0},
	{64, 	NULL, 0},
	{80, 	NULL, 0},
	{96, 	NULL, 0},
	{112, 	NULL, 0},
	{128, 	NULL, 0},
	{144, 	NULL, 0},
	{160, 	NULL, 0},
	{176, 	NULL, 0},
	{192, 	NULL, 0},
	{208, 	NULL, 0},
	{224, 	NULL, 0},
	{240, 	NULL, 0},
	{256, 	NULL, 0},
	{272, 	NULL, 0},
	{288, 	NULL, 0},
	{304, 	NULL, 0},
	{320, 	NULL, 0},
	{336, 	NULL, 0},
	{352, 	NULL, 0},
	{368, 	NULL, 0},
	{384, 	NULL, 0},
	{400, 	NULL, 0},
	{416, 	NULL, 0},
	{432, 	NULL, 0},
	{448, 	NULL, 0},
	{464, 	NULL, 0},
	{480, 	NULL, 0},
	{496, 	NULL, 0},
	{512, 	NULL, 0},
	{0, 	NULL, 0},		/* size > 512 and freed by minos will in here */
	{0, 	NULL, 0}		/* size > 512 will store here used as cache pool*/
};

static LIST_HEAD(host_list);

#define PAGE_ADDR(base, bit)		(base + (bit << PAGE_SHIFT))

#define SLAB_MIN_DATA_SIZE		(16)
#define SLAB_MIN_DATA_SIZE_SHIFT	(4)
#define SLAB_HEADER_SIZE		sizeof(struct slab_header)
#define SLAB_MIN_SIZE			(SLAB_MIN_DATA_SIZE + SLAB_HEADER_SIZE)
#define SLAB_MAGIC			(0xdeadbeef)
#define SLAB_SIZE(size)			((size) + SLAB_HEADER_SIZE)
#define SLAB_HEADER_TO_ADDR(header)	\
	((void *)((unsigned long)header + SLAB_HEADER_SIZE))
#define ADDR_TO_SLAB_HEADER(base) \
	((struct slab_header *)((unsigned long)base - SLAB_HEADER_SIZE))

#define FREE_POOL_OFFSET		(2)
#define CACHE_POOL_OFFSET		(1)

static int inline add_page_section(phy_addr_t base, size_t size)
{
	struct mem_section *ms;
	phy_addr_t mem_base, mem_end;
	size_t real_size, left;

	if ((size == 0) || (nr_sections >= MAX_MEM_SECTIONS)) {
		pr_err("no enough memory section for page section\n");
		return -EINVAL;
	}

	mem_end = ALIGN(base + size, PAGE_SIZE);
	mem_base = BALIGN(base, PAGE_SIZE);
	real_size = mem_end - mem_base;

	if (real_size == 0) {
		add_slab_mem(base, size);
		return 0;
	}

	pr_info("PAGE  SECTION: 0x%p ---> 0x%p [0x%x]\n", mem_base,
			mem_base + real_size, real_size);

	ms = &mem_sections[nr_sections];
	spin_lock_init(&ms->lock);

	/* nr_blocks or free_blocks means the pages count */
	ms->phy_base = mem_base;
	ms->size = real_size;
	ms->type = MEM_SECTION_F_NORMAL | MEM_SECTION_F_PAGE;
	ms->nr_blocks = real_size >> PAGE_SHIFT;
	ms->free_blocks = ms->nr_blocks;
	ms->id = nr_sections;

	/* init the bitmaps and pages data for this section */
	ms->bitmap = alloc_boot_mem(BITS_TO_LONGS(ms->nr_blocks));
	if (!ms->bitmap) {
		pr_err("no enough boot mem for section bitmap\n");
		return -ENOMEM;
	}

	memset(ms->bitmap, 0, BITS_TO_LONGS(ms->nr_blocks));
	ms->bm_current = 0;
	ms->bm_end = ms->nr_blocks;

	ms->pages = alloc_boot_mem(ms->nr_blocks * sizeof(struct page));
	if (!ms->pages) {
		pr_err("no enough boot mem for section pages\n");
		return -ENOMEM;
	}

	/* memset(ms->pages, 0, ms->nr_blocks * sizeof(struct page)); */
	nr_sections++;

	left = mem_base - base;
	add_slab_mem(base, left);

	left = (base + size) - mem_end;
	add_slab_mem(base, left);

	return 0;
}

static int add_block_section(phy_addr_t base, size_t size, int type)
{
	struct mem_section *ms;
	phy_addr_t mem_end, mem_base;
	size_t real_size, left;

	if ((size == 0) || (nr_sections >= MAX_MEM_SECTIONS)) {
		pr_err("no enough memory section for block section\n");
		return -EINVAL;
	}

	mem_end = ALIGN(base + size, MEM_BLOCK_SIZE);
	mem_base = BALIGN(base, MEM_BLOCK_SIZE);
	real_size = mem_end - mem_base;

	/*
	 * if the memory is smaller than BLOCK_SIZE, if the size
	 * is bigger than 32K, add it to a block list for page
	 * allocater, otherwise add it to the slab lis
	 */
	if (real_size == 0) {
		add_page_section(base, size);
		return 0;
	}

	ms = &mem_sections[nr_sections];
	spin_lock_init(&ms->lock);

	ms->phy_base = mem_base;
	ms->size = real_size;
	ms->type = (type & MEM_SECTION_TYPE_MASK) | MEM_SECTION_F_BLOCK;
	ms->nr_blocks = real_size >> MEM_BLOCK_SHIFT;
	ms->free_blocks = ms->nr_blocks;
	ms->id = nr_sections;
	free_blocks += ms->nr_blocks;
	nr_sections++;

	pr_info("BLOCK SECTION: 0x%p ---> 0x%p [0x%p]\n", mem_base,
			mem_base + size, size);

	/*
	 * handle the memory space if the section is not
	 * BLOCK align first is the header section, then
	 * is the tail section
	 */
	left = mem_base - base;
	if (left > 0)
		add_page_section(base, left);

	left = (base + size) - mem_end;
	if (left > 0)
		add_page_section(mem_base + real_size, left);

	return 0;
}

static void parse_system_regions(void)
{
	int type;
	struct memory_region *region;

	list_for_each_entry(region, &mem_list, list) {
		type = memory_region_type(region);
		if ((type == MEMORY_REGION_TYPE_RSV) ||
			(type == MEMORY_REGION_TYPE_VM) ||
			(type == MEMORY_REGION_TYPE_DTB))
			continue;

		add_block_section(region->vir_base, region->size, type);
	}
}

static unsigned long blocks_bitmap_init(unsigned long base)
{
	int i;
	struct mem_section *section;
	unsigned long bitmap_base = base;

	/*
	 * allocate memory for bitmap
	 */
	for (i = 0; i < nr_sections; i++) {
		section = &mem_sections[i];
		if (!(section->type & MEM_SECTION_F_BLOCK))
			continue;

		section->bitmap = (unsigned long *)bitmap_base;
		section->bm_current = 0;
		section->bm_end = section->nr_blocks;
		bitmap_base += BITS_TO_LONGS(section->nr_blocks) *
			sizeof(unsigned long);
	}

	memset((void *)base, 0, bitmap_base - base);

	return bitmap_base;
}

static unsigned long blocks_table_init(unsigned long base)
{
	int i;
	struct mem_section *section;
	unsigned long tmp = base;

	/* allocate memory for mem_block table need confirm */
	for (i = 0; i < nr_sections; i++) {
		section = &mem_sections[i];
		if (!(section->type & MEM_SECTION_F_BLOCK))
			continue;

		section->blocks = (struct mem_block *)tmp;
		tmp += (section->nr_blocks) * sizeof(struct mem_block);
		tmp = BALIGN(tmp, sizeof(unsigned long));
	}

	/* memset((void *)base, 0, tmp - base); */

	return tmp;
}

static int mem_sections_init(void)
{
	int i;
	size_t boot_block_size;
	unsigned long mem_start, mem_end;
	struct mem_section *section = NULL;
	struct mem_section tmp_section;
	struct mem_section *boot_section = NULL;
	unsigned long code_base = CONFIG_MINOS_START_ADDRESS;
	struct mem_block *block;

	mem_start = (unsigned long)bootmem_end;
	pr_info("code_start:0x%x code_end:0x%x\n",
			code_base, mem_start);

	mem_start = BALIGN(mem_start, sizeof(unsigned long));
	mem_start = blocks_table_init(mem_start);

	mem_start = BALIGN(mem_start, sizeof(unsigned long));
	mem_start = blocks_bitmap_init(mem_start);
	pr_info("code_start after blocks_init:0x%x\n", mem_start);

	/*
	 * find the boot section, boot section need always MEM_BLOCK
	 * align, other section may not mapped
	 */
	for (i = 0; i < nr_sections; i++) {
		section = &mem_sections[i];
		if ((code_base >= section->phy_base) && (code_base <
				(section->phy_base + section->size))) {
			boot_section = section;
			break;
		}
	}

	if (!boot_section)
		panic("errors in memory config no boot section\n");

	/*
	 * boot section must be the first entry since when map the memory
	 * need to allocate the page table, at that time only boot section
	 * has been mapped
	 */
	if (boot_section->id != 0) {
		i = boot_section->id;
		memset(&tmp_section, 0, sizeof(struct mem_section));
		memcpy(&tmp_section, &mem_sections[0], sizeof(struct mem_section));
		memcpy(&mem_sections[0], boot_section, sizeof(struct mem_section));
		memcpy(&mem_sections[i], &tmp_section, sizeof(struct mem_section));
		mem_sections[i].id = i;
		mem_sections[0].id = 0;
	}

	mem_end = BALIGN(mem_start, MEM_BLOCK_SIZE);
	pr_info("minos : free_memory_start:0x%x\n", mem_end);

	/*
	 * deal with the boot section, need to update the
	 * information of boot section of hypervisor, since
	 * the text and data memory is in boot section
	 */
	section = &mem_sections[0];
	boot_block_size = (mem_end - code_base) >> MEM_BLOCK_SHIFT;
	free_blocks -= boot_block_size;
	section->free_blocks -= boot_block_size;
	block = section->blocks;

	for (i = 0; i < boot_block_size; i++) {
		set_bit(i, section->bitmap);
		init_list(&block->list);
		block->free_pages = 0;
		block->bm_current = 0;
		block->phy_base = code_base;
		block->flags = 0;

		block++;
		code_base += MEM_BLOCK_SIZE;
	}

	section->bm_current = boot_block_size;

	/*
	 * put this page to the slab allocater do not
	 * waste the memory
	 */
	boot_block_size = mem_end - mem_start;
	if (boot_block_size > SLAB_MIN_SIZE )
		add_slab_mem(mem_start, boot_block_size);

	/* add left bootmem to slab allocator */
	boot_block_size = bootmem_page_base - bootmem_start;
	if (boot_block_size > SLAB_MIN_SIZE) {
		pr_info("add left boot mem to slab 0x%p->0x%x\n",
				bootmem_start, boot_block_size);
		add_slab_mem((unsigned long)bootmem_start,
				boot_block_size);
	}

	bootmem_size = 0;
	bootmem_start = NULL;

	return 0;
}

static struct mem_section *addr_to_mem_section(unsigned long addr)
{
	int i;
	struct mem_section *section = NULL, *temp;

	for (i = 0; i < nr_sections; i++) {
		temp = &mem_sections[i];
		if (IN_RANGE_UNSIGNED(addr, temp->phy_base, temp->size)) {
			section = temp;
			break;
		}
	}

	return section;
}

static struct mem_block *addr_to_mem_block(unsigned long addr)
{
	struct mem_section *section = NULL;

	section = addr_to_mem_section(addr);
	if (!section || !(section->type & MEM_SECTION_F_BLOCK))
		return NULL;

	return (&section->blocks[(addr - section->phy_base) >>
			MEM_BLOCK_SHIFT]);
}

static inline struct mem_section *block_to_mem_section(struct mem_block *block)
{
	return addr_to_mem_section(block->phy_base);
}

static inline unsigned long
offset_in_block_bitmap(unsigned long addr, struct mem_block *block)
{
	return (addr - block->phy_base) >> PAGE_SHIFT;
}

static inline unsigned long
offset_in_section_bitmap(unsigned long addr, struct mem_section *section)
{
	return (addr - section->phy_base) >> MEM_BLOCK_SHIFT;
}

static struct mem_block *
__alloc_mem_block(struct mem_section *section, unsigned long f)
{
	unsigned long bit;
	struct mem_block *block;

	spin_lock(&section->lock);
	if (section->free_blocks == 0) {
		spin_unlock(&section->lock);
		return NULL;
	}

	bit = find_next_zero_bit_loop(section->bitmap, section->nr_blocks,
			section->bm_current);
	if (bit >= section->nr_blocks) {
		pr_warn("section->free_blocks may be incorrect %d\n",
				section->free_blocks);
		spin_unlock(&section->lock);
		return NULL;
	}

	section->free_blocks--;
	section->bm_current++;
	if (section->bm_current >= section->nr_blocks)
		section->bm_current = 0;

	/*
	 * can release the spin lock after set the related
	 * bit to 1
	 */
	set_bit(bit, section->bitmap);
	free_blocks--;
	spin_unlock(&section->lock);

	block = &section->blocks[bit];
	memset(block, 0, sizeof(struct mem_block));
	block->free_pages = PAGES_IN_BLOCK;
	block->flags = f & GFB_MASK;
	block->phy_base = section->phy_base + bit * MEM_BLOCK_SIZE;

	return block;
}

struct mem_block *alloc_mem_block(unsigned long flags)
{
	int i;
	unsigned long f = 0;
	struct mem_block *block = NULL;
	struct mem_section *section;

	for (i = 0; i < nr_sections; i++) {
		section = &mem_sections[i];
		if (!(section->type & MEM_SECTION_F_BLOCK))
			continue;

		block = __alloc_mem_block(section, flags);
		if (block)
			break;
	}

	/*
	 * if the block is not for guest vm mapped it to host
	 * memory space, even the memory is from normal to IO
	 * need to do this
	 */
	if (block && (!(flags & GFB_VM))) {
		if (flags & GFB_IO)
			f |= VM_IO;
		else
			f |= VM_NORMAL;

		/* if the flags is normal memory skip mapping */
		if (f & VM_NORMAL)
			return block;

		create_host_mapping(block->phy_base, block->phy_base,
				MEM_BLOCK_SIZE, f);
	}

	return block;
}

static unsigned long *get_page_meta(struct page_pool *pool)
{
	int bit;
	struct mem_block *block = NULL, *n = NULL;

	list_for_each_entry_safe(block, n, &pool->meta_list, list) {
		bit = find_next_zero_bit_loop(block->pages_bitmap,
				PAGE_METAS_IN_BLOCK, block->bm_current);
		if (bit >= PAGE_METAS_IN_BLOCK) {
			pr_err("block meta free_pages is not correct\n");
			list_del(&block->list);
			continue;
		}

		block->free_pages--;
		if (block->free_pages == 0)
			list_del(&block->list);

		block->bm_current = bit + 1;
		if (block->bm_current == PAGE_METAS_IN_BLOCK)
			block->bm_current = 0;
		set_bit(bit, block->pages_bitmap);

		return (unsigned long *)(block->phy_base + bit * PAGE_META_SIZE);
	}

	block = alloc_mem_block(GPF_PAGE_META);
	if (!block)
		return NULL;

	block->free_pages = PAGE_METAS_IN_BLOCK - 1;
	block->pages_bitmap = (unsigned long *)(block->phy_base +
			MEM_BLOCK_SIZE - PAGE_METAS_BITMAP_SIZE);
	memset(block->pages_bitmap, 0, PAGE_METAS_BITMAP_SIZE);
	set_bit(0, block->pages_bitmap);
	block->bm_current = 1;
	list_add(&pool->meta_list, &block->list);
	pool->meta_blocks++;

	return (unsigned long *)block->phy_base;
}

static inline void *block_meta_base(struct mem_block *block)
{
	return ((void *)block->pages_bitmap + BLOCK_BITMAP_SIZE);
}

static struct page *alloc_pages_from_block(struct mem_block *block,
		int count, int align)
{
	int bit, i;
	void *addr;
	struct page *meta, *page;

	if (block->free_pages < count)
		return NULL;

	if (count == 1)
		bit = find_next_zero_bit(block->pages_bitmap,
				PAGES_IN_BLOCK, block->bm_current);
	else
		bit = bitmap_find_next_zero_area_align(block->pages_bitmap,
				PAGES_IN_BLOCK, 0, count, align);

	if (bit >= PAGES_IN_BLOCK)
		return NULL;

	/*
	 * update the meta info for this block
	 */
	block->free_pages -= count;
	bitmap_set(block->pages_bitmap, bit, count);
	if ((count == 1) || (align == 1)) {
		block->bm_current = bit + count;
		if (block->bm_current == PAGES_IN_BLOCK)
			block->bm_current = 0;
	}

	meta = (struct page *)block_meta_base(block);
	meta += bit;
	page = meta;
	addr = (void *)PAGE_ADDR(block->phy_base, bit);
	meta->phy_base = (unsigned long)addr | (count & 0xfff);
	for (i = 1; i < count; i++) {
		meta++;
		meta->phy_base = (unsigned long)(addr + PAGE_SIZE * i) | 0xfff;
	}

	return page;
}

static struct page *alloc_pages_from_pool(struct page_pool *pool,
		int count, int align, unsigned long flags)
{
	struct mem_block *block = NULL, *n = NULL;
	unsigned long *page_meta = NULL;
	struct page *page = NULL;

	spin_lock(&pool->lock);

	list_for_each_entry_safe(block, n, &pool->block_list, list) {
		page = alloc_pages_from_block(block, count, align);
		if (page) {
			/*
			 * if the block has no more pages for allocate
			 * delete it from the pool, then do not search
			 * for pages in this block, when someone free
			 * some pages, it will be added to the pool again
			 */
			if (block->free_pages == 0)
				list_del(&block->list);
			break;
		}
	}

	if (page)
		goto out;

	/* need new memory block from the section */
	block = alloc_mem_block(flags);
	if (!block)
		goto out;

	page_meta = get_page_meta(pool);
	if (!page_meta)
		goto out;

	memset(page_meta, 0, PAGE_META_SIZE);
	block->pages_bitmap = page_meta;
	list_add(&pool->block_list, &block->list);
	pool->page_blocks++;

	page = alloc_pages_from_block(block, count, align);

out:
	spin_unlock(&pool->lock);
	return page;
}

static struct page *__alloc_pages_from_section(struct mem_section *section,
		int count, int align)
{
	int bit, i;
	void *addr;
	struct page *page, *tmp;

	if (count == 1)
		bit = find_next_zero_bit(section->bitmap, section->nr_blocks,
				section->bm_current);
	else
		bit = bitmap_find_next_zero_area_align(section->bitmap,
				section->nr_blocks, 0, count, align);
	if (bit >= section->nr_blocks)
		return NULL;

	bitmap_set(section->bitmap, bit, count);
	if ((count = 1) || (align == 1)) {
		section->bm_current = bit + count;
		if (section->bm_current >= section->nr_blocks)
			section->bm_current = 0;
	}

	tmp = page = section->pages + bit;
	addr = (void *)PAGE_ADDR(section->phy_base, bit);
	page->phy_base = (unsigned long)addr | (count & 0xfff);
	section->free_blocks -= count;

	/* indicate that other pages are not a page header */
	for (i = 1; i < count; i++) {
		tmp++;
		tmp->phy_base = (unsigned long)(addr + PAGE_SIZE * i) | 0xfff;
	}

	return page;
}

static struct page *alloc_pages_from_section(int pages, int align)
{
	int i;
	struct page *page = NULL;
	struct mem_section *section;

	for (i = 0; i < nr_sections; i++) {
		section = &mem_sections[i];
		if (!(section->type & MEM_SECTION_F_PAGE))
			continue;

		spin_lock(&section->lock);
		if (section->free_blocks < pages) {
			spin_unlock(&section->lock);
			continue;
		}

		page = __alloc_pages_from_section(section, pages, align);
		spin_unlock(&section->lock);

		if (page)
			return page;
	}

	return NULL;
}

struct page *__alloc_pages(int pages, int align)
{
	struct page *page;

	if ((pages <= 0) || (align == 0))
		return NULL;

	/* can not make sure section's page can keep align */
	if (align > 1)
		goto from_pool;

	page = alloc_pages_from_section(pages, align);
	if (page)
		return page;

from_pool:
	return alloc_pages_from_pool(page_pool, pages, align, GFB_PAGE);
}

void *__get_free_pages(int pages, int align)
{
	struct page *page = NULL;

	page = __alloc_pages(pages, align);
	if (page)
		return (void *)(page->phy_base & __PAGE_MASK);

	return NULL;
}

void *__get_io_pages(int pages, int align)
{
	struct page *page = NULL;

	page = alloc_pages_from_pool(io_pool, pages, align, GFB_PAGE | GFB_IO);
	if (page)
		return (void *)(page->phy_base & __PAGE_MASK);

	return NULL;
}

static size_t inline get_slab_alloc_size(size_t size)
{
	return BALIGN(size, SLAB_MIN_DATA_SIZE);
}

static inline int slab_pool_id(size_t size)
{
	int id = (size >> SLAB_MIN_DATA_SIZE_SHIFT) - 1;

	return id >= (pslab->pool_nr - FREE_POOL_OFFSET) ?
			(pslab->pool_nr - FREE_POOL_OFFSET) : id;
}

void add_slab_mem(unsigned long base, size_t size)
{
	int i;
	struct slab_pool *pool;
	struct slab_header *header;

	if (size < SLAB_MIN_SIZE)
		return;

	pr_info("add mem : 0x%x : 0x%x to slab\n", base, size);

	/*
	 * this function will be only called on boot
	 * time and on the cpu 0, so do not need to
	 * aquire the spin lock.
	 */
	if (!(base & (MEM_BLOCK_SIZE - 1)))
		pr_warn("memory may be a block\n");

	pool = &pslab->pool[pslab->pool_nr - FREE_POOL_OFFSET - 1];
	if (size <= SLAB_SIZE(pool->size)) {
		if (size < SLAB_SIZE(SLAB_MIN_DATA_SIZE)) {
			pr_warn("drop small slab memory 0x%p 0x%x\n", base, size);
			return;
		}

		i = size - SLAB_HEADER_SIZE;
		i &= ~(SLAB_MIN_DATA_SIZE - 1);
		if (i < SLAB_MIN_DATA_SIZE)
			return;

		size = i;
		i = slab_pool_id(size);
		pool = &pslab->pool[i];
		header = (struct slab_header *)base;
		header->size = size;
		header->next = NULL;
		add_slab_to_slab_pool(header, pool);
		return;
	}

	/*
	 * the last pool is used to cache pool, if the
	 * slab memory region bigger than 512byte, it will
	 * first used as a cached memory slab
	 */
	pool = &pslab->pool[pslab->pool_nr - CACHE_POOL_OFFSET];
	header = (struct slab_header *)base;
	header->size = size - SLAB_HEADER_SIZE;
	header->next = NULL;
	add_slab_to_slab_pool(header, pool);
}

static int alloc_new_slab_block(void)
{
	struct mem_block *block;

	block = alloc_mem_block(GFB_SLAB);
	if (!block)
		return -ENOMEM;

	list_add_tail(&pslab->block_list, &block->list);

	/*
	 * in slab allocator free_pages is ued to count how
	 * many slabs has been allocated out, if the count
	 * is 1, then this block can return back tho the
	 * section
	 */
	block->free_pages = 1;
	pslab->free_size = MEM_BLOCK_SIZE;
	pslab->slab_free = block->phy_base;
	pslab->alloc_blocks++;

	return 0;
}

static inline struct slab_header *
get_slab_from_slab_pool(struct slab_pool *pool)
{
	struct slab_header *header;

	header = pool->head;
	pool->head = header->next;
	header->magic = SLAB_MAGIC;
	pool->nr--;

	return header;
}

static void inline add_slab_to_slab_pool(struct slab_header *header,
		struct slab_pool *pool)
{
	header->next = pool->head;
	pool->head = header;
	pool->nr++;
}

static void *get_slab_from_pool(size_t size)
{
	struct slab_pool *slab_pool;
	struct slab_header *header;
	int id = slab_pool_id(size);

	/* if big slab return directly */
	if (id >= (pslab->pool_nr - FREE_POOL_OFFSET))
		return NULL;

	slab_pool = &pslab->pool[id];
	if (!slab_pool->head)
		return NULL;

	header = get_slab_from_slab_pool(slab_pool);

	return SLAB_HEADER_TO_ADDR(header);
}

static void *get_slab_from_big_pool(size_t size)
{
	struct slab_pool *slab_pool;
	struct slab_header *header, *prev;

	/* small slab size return directly */
	if (slab_pool_id(size) < (pslab->pool_nr - FREE_POOL_OFFSET))
		return NULL;

	slab_pool = &pslab->pool[pslab->pool_nr - FREE_POOL_OFFSET];
	header = prev = slab_pool->head;

	/*
	 * get_slab_from_big_pool will find the slab which its
	 * size is equal to the new request slab's size, this
	 * can allocate the slab which is freed by someone
	 */
	while (header != NULL) {
		if (header->size == size) {
			/* return the header and delete it from the pool */
			if (header == prev)
				slab_pool->head = header->next;
			else
				prev->next = header->next;

			header->magic = SLAB_MAGIC;
			return SLAB_HEADER_TO_ADDR(header);
		} else {
			prev = header;
			header = header->next;
			continue;
		}
	}

	return NULL;
}

static void *get_slab_from_slab_free(size_t size)
{
	struct slab_header *header;
	size_t request_size = SLAB_SIZE(size);
	struct mem_block *block;

	if (pslab->free_size < request_size)
		return NULL;

	header = (struct slab_header *)pslab->slab_free;
	memset(header, 0, sizeof(struct slab_header));
	header->size = size;
	header->magic = SLAB_MAGIC;

	block = addr_to_mem_block(pslab->slab_free);
	pslab->slab_free += request_size;
	pslab->free_size -= request_size;

	if (pslab->free_size < SLAB_SIZE(SLAB_MIN_DATA_SIZE)) {
		header->size += pslab->free_size;
		pslab->slab_free = 0;
		pslab->free_size = 0;
	}

	block->free_pages++;

	return SLAB_HEADER_TO_ADDR(header);
}

static void *get_new_slab(size_t size)
{
	int id;
	struct slab_pool *pool;
	struct slab_header *header;
	struct mem_block *block;
	static int times = 0;

	if (pslab->alloc_blocks > CONFIG_MAX_SLAB_BLOCKS) {
		if (times == 0)
			pr_warn("slab pool block size is bigger than %d\n",
					CONFIG_MAX_SLAB_BLOCKS);
		times++;
		return NULL;
	}

	if (pslab->free_size >= SLAB_SIZE(SLAB_MIN_DATA_SIZE)) {
		/* left memory if is a big slab push to cache pool */
		id = slab_pool_id(pslab->free_size - SLAB_HEADER_SIZE);
		if (id >= pslab->pool_nr - FREE_POOL_OFFSET)
			id = pslab->pool_nr - CACHE_POOL_OFFSET;
		pool = &pslab->pool[id];

		block = addr_to_mem_block(pslab->slab_free);

		header = (struct slab_header *)pslab->slab_free;
		memset(header, 0, SLAB_HEADER_SIZE);
		header->size = pslab->free_size - SLAB_HEADER_SIZE;
		header->magic = SLAB_MAGIC;
		add_slab_to_slab_pool(header, pool);

		pslab->free_size = 0;
		pslab->slab_free = 0;

		block->free_pages++;
	}

	if (alloc_new_slab_block())
		return NULL;

	return get_slab_from_slab_free(size);
}

static void *get_slab_from_cache(size_t size)
{
	struct slab_pool *slab_pool;
	struct slab_header *prev;
	struct slab_header *header;
	struct slab_header *ret = NULL;
	struct slab_header *new;
	uint32_t left_size;
	unsigned long base;

	slab_pool = &pslab->pool[pslab->pool_nr - CACHE_POOL_OFFSET];
	header = slab_pool->head;
	prev = slab_pool->head;

	while (header != NULL) {
		if (header->size < size) {
			prev = header;
			header = header->next;
			continue;
		}

		/*
		 * split the chache slab, the memory will split
		 * from the head of the slab
		 */
		base = (unsigned long)header + SLAB_SIZE(size);
		left_size = header->size - size;

		ret = header;

		/*
		 * check the left size, if the the left size smaller
		 * than SLAB_MIN_DATA_SIZE, drop it
		 */
		if (left_size <= SLAB_SIZE(80)) {
			if (prev == header)
				slab_pool->head = header->next;
			else
				prev->next = header->next;

			add_slab_mem(base, left_size);
		} else {
			new = (struct slab_header *)base;
			new->next = header->next;
			new->size = left_size - SLAB_HEADER_SIZE;

			if (prev == header)
				slab_pool->head = new;
			else
				prev->next = new;
		}

		ret->size = size;
		ret->magic = SLAB_MAGIC;
		break;
	}

	if (ret)
		return SLAB_HEADER_TO_ADDR(ret);

	return NULL;
}

static void *get_big_slab(size_t size)
{
	struct slab_pool *slab_pool;
	struct slab_header *header;
	int id = slab_pool_id(size);

	while (1) {
		slab_pool = &pslab->pool[id];
		if (!slab_pool->head) {
			id++;
			if (id == (pslab->pool_nr - CACHE_POOL_OFFSET))
				return NULL;
		} else {
			break;
		}
	}

	header = (struct slab_header *)get_slab_from_slab_pool(slab_pool);

	return SLAB_HEADER_TO_ADDR(header);
}

typedef void *(*slab_alloc_func)(size_t size);

static slab_alloc_func alloc_func[] = {
	get_slab_from_pool,
	get_slab_from_big_pool,
	get_slab_from_cache,
	get_slab_from_slab_free,
	get_new_slab,
	get_big_slab,
	NULL,
};

void *malloc(size_t size)
{
	int i = 0;
	void *ret = NULL;
	slab_alloc_func func;

	if (size == 0)
		return NULL;

	size = get_slab_alloc_size(size);
	spin_lock(&pslab->lock);

	while (1) {
		func = alloc_func[i];
		if (!func)
			break;

		ret = func(size);
		if (ret)
			break;
		i++;
	}

	spin_unlock(&pslab->lock);

	return ret;
}

void *zalloc(size_t size)
{
	void *base;

	base = malloc(size);
	if (!base)
		return NULL;

	memset(base, 0, size);
	return base;
}

void release_mem_block(struct mem_block *block)
{
	unsigned long start;
	struct mem_section *section = NULL;
	struct mem_block *meta_block = NULL;

	if (!block)
		return;

	if (block->free_pages < PAGES_IN_BLOCK)
		goto out;

	section = block_to_mem_section(block);
	spin_lock(&section->lock);
	start = offset_in_section_bitmap(block->phy_base, section);
	bitmap_clear(section->bitmap, start, 1);
	free_blocks++;

	/*
	 * shoud do this here or in free_page function ?
	 * TBD fix me
	 */
	if (block->flags & (GFB_PAGE)) {
		meta_block = addr_to_mem_block((unsigned long)block->pages_bitmap);
		if (!meta_block)
			goto out;
		start = ((unsigned long)(block->pages_bitmap) -
				block->phy_base) / PAGE_META_SIZE;
		clear_bit(start, meta_block->pages_bitmap);
	}
out:
	spin_unlock(&section->lock);
}

static void free_pages_in_block(void *addr, struct mem_block *block)
{
	unsigned long start;
	struct page *meta;
	int i, count;
	struct page_pool *pool;

	if (!(block->flags & GFB_PAGE)) {
		pr_err("addr is not a page 0x%p\n", addr);
		return;
	}

	start = offset_in_block_bitmap((unsigned long)addr, block);

	if (block->flags & GFB_IO)
		pool = io_pool;
	else
		pool = page_pool;

	spin_lock(&pool->lock);

	meta = (struct page *)block_meta_base(block);
	count = meta->phy_base & 0xfff;
	i = block->free_pages;
	block->free_pages += count;

	/*
	 * add the block to the page pool if the
	 * pages free is not 0
	 */
	if (i == 0)
		list_add_tail(&pool->block_list, &block->list);

	bitmap_clear(block->pages_bitmap, start, count);
	for (i = start; i < (start + count); i++) {
		meta->phy_base = 0;
		meta++;
	}

	spin_unlock(&pool->lock);
}

static int free_pages_in_section(void *addr, struct mem_section *ms)
{
	struct page *page;
	unsigned long start;
	int count;

	spin_lock(&ms->lock);

	start = ((unsigned long)addr - ms->phy_base) >> PAGE_SHIFT;
	page = ms->pages + start;
	count = page->phy_base && 0xffff;

	/* just clear the bitmap from start with count */
	bitmap_clear(ms->bitmap, start, count);
	ms->free_blocks += count;

	spin_unlock(&ms->lock);

	return 0;
}

void free_pages(void *addr)
{
	struct mem_block *block;
	struct mem_section *section;

	section = addr_to_mem_section((vir_addr_t)addr);
	if (section && (section->type & MEM_SECTION_F_PAGE)) {
		free_pages_in_section(addr, section);
		return;
	}

	block = addr_to_mem_block((vir_addr_t)addr);
	if (block && (block->flags & GFB_PAGE)) {
		free_pages_in_block(addr, block);
		return;
	}

	pr_err("addr-0x%p is not a page address\n", addr);
}

void release_pages(struct page *page)
{
	return free_pages((void *)(page->phy_base & __PAGE_MASK));
}

void free(void *addr)
{
	int id;
	struct slab_header *header;
	struct slab_pool *slab_pool;
	struct mem_block *block;
	struct mem_section *section;

	if (!addr)
		return;

	/*
	 * check whether this addr is in page section or
	 * block section or is just a slab memory
	 */
	section = addr_to_mem_section((unsigned long)addr);
	if (!section)
		goto free_slab;

	if (section->type & MEM_SECTION_F_PAGE) {
		free_pages_in_section(addr, section);
		return;
	}

	block = addr_to_mem_block((unsigned long)addr);
	if (block && (block->flags & GFB_PAGE)) {
		free_pages_in_block(addr, block);
		return;
	}

free_slab:
	header = ADDR_TO_SLAB_HEADER(addr);
	if (header->magic != SLAB_MAGIC) {
		pr_warn("memory is not a slab mem 0x%p\n",
				(unsigned long)addr);
		return;
	}

	/* big slab will default push to free cache pool */
	spin_lock(&pslab->lock);
	id = slab_pool_id(header->size);
	slab_pool = &pslab->pool[id];
	add_slab_to_slab_pool(header, slab_pool);
	spin_unlock(&pslab->lock);
}

static void slab_init(void)
{
	pr_info("slab memory allocator init...\n");
	memset(pslab, 0, sizeof(struct slab));
	init_list(&pslab->block_list);
	pslab->pool = slab_pool;
	pslab->pool_nr = sizeof(slab_pool) / sizeof(slab_pool[0]);
	spin_lock_init(&pslab->lock);
}

static void page_pool_init(void)
{
	memset(page_pool, 0, sizeof(struct page_pool));
	spin_lock_init(&page_pool->lock);
	init_list(&page_pool->meta_list);
	init_list(&page_pool->block_list);

	memset(io_pool, 0, sizeof(struct page_pool));
	spin_lock_init(&io_pool->lock);
	init_list(&io_pool->meta_list);
	init_list(&io_pool->block_list);
}

int has_enough_memory(size_t size)
{
	return (free_blocks >= (size >> MEM_BLOCK_SHIFT));
}

int mm_do_init(void)
{
	/*
	 * first need init the slab allocator then
	 * parsing the memroy region to the hypervisor
	 * and convert it to the memory section
	 */
	pr_info("dynamic memory allocator init..\n");

	slab_init();
	page_pool_init();
	parse_system_regions();
	mem_sections_init();

	return 0;
}
