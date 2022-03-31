#ifndef _MINOS_TYPES_H_
#define _MINOS_TYPES_H_

#include <asm/asm_types.h>
#include <asm/barrier.h>
#include <minos/compiler.h>
#include <minos/const.h>
#include <config/config.h>

typedef __u32	u32;
typedef __s32	s32;
typedef __u16	u16;
typedef __s16	s16;
typedef __u8	u8;
typedef __s8	s8;
typedef __u64	u64;
typedef __s64	s64;

typedef __u32	uint32_t;
typedef __s32	int32_t;
typedef __u16	uint16_t;
typedef __s16	int16_t;
typedef __u8	uint8_t;
typedef __s8	int8_t;
typedef __u64	uint64_t;
typedef __s64	int64_t;

typedef long ssize_t;
typedef unsigned long size_t;

typedef unsigned long phy_addr_t;
typedef unsigned long virt_addr_t;
typedef unsigned long paddr_t;
typedef unsigned long vaddr_t;

typedef int16_t handle_t;
typedef uint32_t right_t;

typedef int16_t tid_t;
typedef int16_t pid_t;

typedef unsigned long uintptr_t;
typedef int irqreturn_t;

typedef int bool;

typedef uint64_t pgd_t;
typedef uint64_t pud_t;
typedef uint64_t pmd_t;
typedef uint64_t pte_t;

enum {
	false = 0,
	true  = 1,
};

#define FILENAME_MAX	256

#define ULONG(v)	((unsigned long)(v))

#define MAX(a, b)	(a) > (b) ? (a) : (b)
#define MIN(a, b)	(a) < (b) ? (a) : (b)
#define max(a, b)	(a) > (b) ? (a) : (b)
#define min(a, b)	(a) < (b) ? (a) : (b)

#define ERROR_PTR(value)	\
	(void *)(unsigned long)(value)

#define IS_ERROR_PTR(ptr)	\
	(((long)(ptr)) > -4096 && ((long)(ptr) < 4096))

#define u8_to_u16(low, high)	((high << 8) | low)
#define u8_to_u32(u1, u2, u3, u4)	\
	((u4 << 24) | (u3 << 16) | (u2 << 8) | (u1))
#define u16_to_u32(low, high)	((high << 16) | low)

#define offsetof(TYPE, MEMBER)  ((size_t)&((TYPE *)0)->MEMBER)

#define NULL ((void *)0)
typedef void (*void_func_t)(void);

#define container_of(ptr, name, member) \
	(name *)((unsigned char *)ptr - ((unsigned char *)&(((name *)0)->member)))

#define BIT(nr) (ULONG(1) << (nr))

#define stringify_no_expansion(x) #x
#define stringify(x) stringify_no_expansion(x)

#define SIZE_1G		(0x40000000UL)
#define SIZE_4K		(0x1000)
#define SIZE_1M		(0x100000)
#define SIZE_1K		(0x400)

#define SIZE_16K	(16 * SIZE_1K)
#define SIZE_32M	(32 * SIZE_1M)
#define SIZE_64K	(64 * SIZE_1K)
#define SIZE_512M	(512 * SIZE_1M)
#define SIZE_2M		(2 * 1024 * 1024)
#define SIZE_8M		(8 * 1024 * 1024)

#define PAGE_SIZE	(SIZE_4K)
#define PAGE_SHIFT	(12)
#define PAGE_MASK	(0xfffUL)

#define BLOCK_SIZE	(0x200000)
#define BLOCK_SHIFT	(21)

#define PAGES_PER_BLOCK	(BLOCK_SIZE >> PAGE_SHIFT)

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define INVALID_ADDR	0

#define BITS_PER_BYTE	(8)

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

#define BITS_TO_LONGS(nr) DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))

#define DECLARE_BITMAP(name, bits) \
	unsigned long name[BITS_TO_LONGS(bits)]

#define BITMAP_SIZE(size)	(BITS_TO_LONGS((size)) * sizeof(long))

#define BITS_PER_LONG		64
#define BIT_ULL(nr)		(1ULL << (nr))
#define BIT_MASK(nr)		(1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)
#define BIT_ULL_MASK(nr)	(1ULL << ((nr) % BITS_PER_LONG_LONG))
#define BIT_ULL_WORD(nr)	((nr) / BITS_PER_LONG_LONG)

#define __round_mask(x, y) 	((__typeof__(x))((y)-1))
#define round_up(x, y) 		((((x)-1) | __round_mask(x, y))+1)
#define round_down(x, y) 	((x) & ~__round_mask(x, y))

#define ALIGN(x, y)	((x) & ~__round_mask(x, y))
#define BALIGN(x, y)	(((x) + (y) - 1) & ~__round_mask(x, y))

#define PAGE_BALIGN(x)	BALIGN((unsigned long)(x), PAGE_SIZE)
#define PAGE_ALIGN(x)	ALIGN((unsigned long)(x), PAGE_SIZE)

#define PAGE_NR(size)	(PAGE_BALIGN(size) >> PAGE_SHIFT)

#define IS_PAGE_ALIGN(x)	(!((unsigned long)(x) & (PAGE_SIZE - 1)))
#define IS_BLOCK_ALIGN(x)	(!((unsigned long)(x) & (0x1fffff)))

#define __IN_RANGE_UNSIGNED(sbase, ssize, dbase, dsize) \
	(((sbase) >= (dbase)) && (((sbase) + (ssize) - 1) < ((dbase) + (dsize))))

#define IN_RANGE_UNSIGNED(sbase, ssize, dbase, dsize) \
	__IN_RANGE_UNSIGNED((unsigned long)(sbase), (unsigned long)(ssize), (unsigned long)(dbase), (size_t)(dsize))

#define __stringify_1(x...) #x
#define __stringify(x...)   __stringify_1(x)

#define GENMASK(h, l) \
    (((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

#define GENMASK_ULL(h, l) \
    (((~0ULL) << (l)) & (~0ULL >> (BITS_PER_LONG - 1 - (h))))

#define INT_MAX 	(2147483647)
#define INT_MIN		(-2147483648)

#define BUG() \
	while (1)

#define NR_CPUS		CONFIG_NR_CPUS

#define BAD_ADDRESS (-1)

#define OS_PRIO_MAX 8

extern int8_t const ffs_one_table[256];

typedef uint32_t flag_t;

typedef struct {
	int value;
} atomic_t;

/*
 * [0  - 15] - current number
 * [16 - 31] - next number
 */
typedef struct spinlock {
#ifdef CONFIG_SMP
	int current_ticket;
	int next_ticket;
#endif
} spinlock_t;

static inline void __write_once_size(volatile void *p, void *res, int size)
{
	switch (size) {
	case 1: *(volatile uint8_t *)p = *(uint8_t *)res; break;
	case 2: *(volatile uint16_t *)p = *(uint16_t *)res; break;
	case 4: *(volatile uint32_t *)p = *(uint32_t *)res; break;
	case 8: *(volatile uint64_t *)p = *(uint64_t *)res; break;
	default:
		barrier();
		__builtin_memcpy((void *)p, (const void *)res, size);
		barrier();
	}
}

#define WRITE_ONCE(x, val) \
({							\
	union { typeof(x) __val; char __c[1]; } __u =	\
		{ .__val = (typeof(x)) (val) }; \
	__write_once_size(&(x), __u.__c, sizeof(x));	\
	__u.__val;					\
})

#endif
