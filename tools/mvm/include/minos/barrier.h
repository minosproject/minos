#ifndef __MVM_BARRIER_H__
#define __MVM_BARRIER_H__

#ifdef __CLANG__
#include <arm_acle.h>
#define isb()		__isb(0xf)
#define dmb(opt)	__dmb(0xf)
#define dsb(opt)	__dsb(0xf)
#else
#define isb()           asm volatile("isb" : : : "memory")
#define dmb(opt)        asm volatile("dmb " #opt : : : "memory")
#define dsb(opt)        asm volatile("dsb " #opt : : : "memory")
#endif

#define mb()            dsb(sy)
#define rmb()           dsb(ld)
#define wmb()           dsb(st)

#define dma_rmb()       dmb(oshld)
#define dma_wmb()       dmb(oshst)

#define smp_mb()        dmb(ish)
#define smp_rmb()       dmb(ishld)
#define smp_wmb()       dmb(ishst)

#endif
