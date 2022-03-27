#ifndef _MINOS_AARCH64_TYPES_H_
#define _MINOS_AARCH64_TYPES_H_

#include <config/config.h>

typedef unsigned long		__u64;
typedef unsigned int		__u32;
typedef unsigned short		__u16;
typedef unsigned char		__u8;
typedef signed long		__s64;
typedef signed int		__s32;
typedef signed short		__s16;
typedef signed char		__s8;

#ifndef __aarch64__
#define __aarch64__
#endif

#ifdef CONFIG_ARM_ADDRESS_TAGGING
#define ptov(addr)	((unsigned long)(addr) | CONFIG_PTOV_MASK)
#define vtop(addr)	((unsigned long)(addr) & CONFIG_VTOP_MASK)
#define __va(addr)	((unsigned long)(addr) & CONFIG_VTOP_MASK)
#define is_kva(va)	(((unsigned long)(va) & CONFIG_PTOV_MASK) == CONFIG_PTOV_MASK)
#else
#define ptov(addr)	((unsigned long)addr)
#define vtop(addr)	((unsigned long)addr)
#define __va(va)	((unsigned long)(va))
#define is_kva(va)	((unsigned long)va >= 4096)
#endif

/*
 * 512G for user space process
 */
#define USER_PROCESS_ADDR_LIMIT		(1UL << 39)

#endif
