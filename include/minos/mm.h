#ifndef _MINOS_MM_H_
#define _MINOS_MM_H_

#include <minos/list.h>
#include <minos/spinlock.h>
#include <config/config.h>

#define PAGE_NR(size)		(size >> PAGE_SHIFT)

#define GFB_SLAB		(1 << 0)
#define GFB_PAGE		(1 << 1)
#define GPF_PAGE_META		(1 << 2)

#define GFB_SLAB_BIT		(0)
#define GFB_PAGE_BIT		(1)
#define GFB_PAGE_META_BIT	(2)

#define GFB_MASK		(0xffff)

struct memtag;
struct vm;

#define MAX_MEM_SECTIONS	(10)
#define MEM_BLOCK_SIZE		(0x200000)
#define MEM_BLOCK_SHIFT		(21)
#define PAGES_IN_BLOCK		(MEM_BLOCK_SIZE >> PAGE_SHIFT)

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

struct mm_struct {
	unsigned long page_table_base;
	struct list_head mem_list;
	spinlock_t lock;
};

struct mem_allocator {
	char *name;
	int (*init)(void);
	void *(*malloc)(size_t size);
	void (*free)(void *addr);
	void *(*get_free_pages_align)(int pages, int align);
	void (*free_pages)(void *addr);
	struct mem_block *(*get_mem_block)(unsigned long flags);
	void (*free_mem_block)(struct mem_block *block);
};

extern struct list_head mem_list;

int mm_init(void);
void *malloc(size_t size);
void *zalloc(size_t size);
void free(void *addr);
void *get_free_pages_align(int pages, int align);
void free_pages(void *addr);

static inline void *get_free_page(void)
{
	return get_free_pages_align(1, 1);
}

static inline void *get_free_pages(int pages)
{
	return get_free_pages_align(pages, 1);
}

int register_memory_region(struct memtag *res);
int vm_mm_init(struct vm *vm);
struct mem_block *alloc_new_mem_block(unsigned long flags);
void free_mem_block(struct mem_block *block);

#endif
