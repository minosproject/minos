#ifndef _MINOS_MM_H_
#define _MINOS_MM_H_

#include <minos/list.h>
#include <minos/spinlock.h>

#define PAGE_NR(size)	(size >> PAGE_SHIFT)

struct memtag;
struct vm;

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

void mm_init(void);
void *malloc(size_t size);
void *zalloc(size_t size);
void *get_free_pages(int pages);
void free(void *addr);
void free_pages(void *addr);

static inline void *get_free_page(void)
{
	return get_free_pages(1);
}

int register_memory_region(struct memtag *res);
int vm_mm_init(struct vm *vm);

#endif
