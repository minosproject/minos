#ifndef _MVISOR_MM_H_
#define _MVISOR_MM_H_

int mm_init(void);
char *vmm_malloc(size_t size);
char *vmm_alloc_pages(int pages);

static inline char *vmm_alloc_page(void)
{
	return vmm_alloc_pages(1);
}

#endif
