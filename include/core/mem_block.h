#ifndef _MVISOR_MEM_BLOCK_H_
#define _MVISOR_MEM_BLOCK_H_

char *vmm_malloc(size_t size);
char *vmm_alloc_pages(int pages);

static inline char *vmm_alloc_page(void)
{
	return vmm_alloc_pages(1);
}

#endif
