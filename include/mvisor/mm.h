#ifndef _MVISOR_MM_H_
#define _MVISOR_MM_H_

#include <mvisor/list.h>

int vmm_mm_init(void);
char *vmm_malloc(size_t size);
char *vmm_alloc_pages(int pages);
void vmm_free(void *addr);
void vmm_free_pages(void *addr);

int vmm_register_memory_region(void *res);

struct memory_region {
	phy_addr_t mem_base;
	phy_addr_t vir_base;
	size_t size;
	int type;
	struct list_head list;
};

struct mm_struct {
	phy_addr_t page_table_base;
	struct list_head mem_list;
};

static inline char *vmm_alloc_page(void)
{
	return vmm_alloc_pages(1);
}

#endif
