#ifndef _IO_H_
#define _IO_H_

#include <core/types.h>

static inline uint8_t __raw_readb(const uint8_t *addr)
{
	return *(const volatile uint8_t *)addr;
}

static inline uint16_t __raw_readw(const uint16_t *addr)
{
	return *(const volatile uint16_t *)addr;
}

static inline uint32_t __raw_readl(const uint32_t *addr)
{
	return *(const volatile uint32_t *)addr;
}

static void __raw_writeb(uint8_t *addr, uint8_t b)
{
	*(volatile uint8_t *)addr = b;
}

static void __raw_writew(uint16_t *addr, uint16_t w)
{
	*(volatile uint16_t *)addr = w;
}

static void __raw_writel(uint32_t *addr, uint32_t l)
{
	*(volatile uint32_t *)addr = l;
}

#define readb	__raw_readb
#define readw	__raw_readw
#define readl	__raw_readl

#define writeb	__raw_writeb
#define writew	__raw_writew
#define writel  __raw_writel

#define ioread8(addr)			readb((uint8_t *)addr)
#define ioread16(addr)			readw((uint16_t *)addr)
#define ioread32(addr)			readl((uint32_t *)addr)

#define iowrite8(addr, v)		writeb((uint8_t *)(addr), (v))
#define iowrite16(addr, v)		writew((uint16_t *)(addr), (v))
#define iowrite32(addr, v)		writel((uint32_t *)(addr), (v))

#endif
