#ifndef _MINOS_IO_H_
#define _MINOS_IO_H_

#include <minos/types.h>

static inline uint8_t __raw_readb(const volatile void *addr)
{
	return *(const volatile uint8_t *)addr;
}

static inline uint16_t __raw_readw(const volatile void *addr)
{
	return *(const volatile uint16_t *)addr;
}

static inline uint32_t __raw_readl(const volatile void *addr)
{
	return *(const volatile uint32_t *)addr;
}

static inline uint64_t __raw_readll(const volatile void *addr)
{
	return *(const volatile uint64_t *)addr;
}

static inline void __raw_writeb(volatile void *addr, uint8_t b)
{
	*(volatile uint8_t *)addr = b;
}

static inline void __raw_writew(volatile void *addr, uint16_t w)
{
	*(volatile uint16_t *)addr = w;
}

static inline void __raw_writel(volatile void *addr, uint32_t l)
{
	*(volatile uint32_t *)addr = l;
}

static inline void __raw_writell(volatile void *addr, uint64_t ll)
{
	*(volatile uint64_t *)addr = ll;
}

#define ioread8(addr)			__raw_readb(addr)
#define ioread16(addr)			__raw_readw(addr)
#define ioread32(addr)			__raw_readl(addr)
#define ioread64(addr)			__raw_readll(addr)

#define iowrite8(addr, v)		__raw_writeb((addr), (v))
#define iowrite16(addr, v)		__raw_writew((addr), (v))
#define iowrite32(addr, v)		__raw_writel((addr), (v))
#define iowrite64(addr, v)		__raw_writell((addr), (v))

#endif
