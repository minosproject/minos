#ifndef _IO_H_
#define _IO_H_

#include <core/types.h>

static inline u8 __raw_readb(const uint8_t *addr)
{
	return *(const volatile u8 *)addr;
}

static inline u16 __raw_readw(const uint16_t *addr)
{
	return *(const volatile u16 *)addr;
}

static inline u32 __raw_readl(const uint32_t *addr)
{
	return *(const volatile u32 *)addr;
}

static void __raw_writeb(uint8_t *addr, u8 b)
{
	*(volatile u8 *)addr = b;
}

static void __raw_writew(uint16_t *addr, u16 w)
{
	*(volatile u16 *)addr = w;
}

static void __raw_writel(uint32_t *addr, u32 l)
{
	*(volatile u32 *)addr = l;
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
