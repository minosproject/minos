#ifndef __MINOS_AARCH64_CACHE_H__
#define __MINOS_AARCH64_CACHE_H__

void flush_dcache_range(unsigned long addr, size_t size);
void inv_dcache_range(unsigned long addr, size_t size);
void flush_cache_all(void);
void flush_dcache_all(void);
void inv_dcache_all(void);

static inline void inv_icache_local(void)
{
	asm volatile("ic iallu");
	dsbsy();
	isb();
}

static inline void inv_icache_all(void)
{
	asm volatile("ic ialluis");
	dsbsy();
}

#endif
