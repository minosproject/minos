#ifndef _MVISOR_MM_H_
#define _MVISOR_MM_H_

#include <mvisor/list.h>

void mvisor_mm_init(void);
char *mvisor_malloc(size_t size);
char *mvisor_zalloc(size_t size);
char *mvisor_alloc_pages(int pages);
void mvisor_free(void *addr);
void mvisor_free_pages(void *addr);

int mvisor_register_memory_region(void *res);

struct memory_region {
	int type;
	unsigned long mem_base;
	unsigned long vir_base;
	size_t size;
	struct list_head list;
};

struct mm_struct {
	unsigned long page_table_base;
	struct list_head mem_list;
};

static inline char *mvisor_alloc_page(void)
{
	return mvisor_alloc_pages(1);
}

#endif
