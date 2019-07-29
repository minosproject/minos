#ifndef __MINOS_ASM_BARRIER_H__
#define __MINOS_ASM_BARRIER_H__

#define __isb()		asm volatile("isb" : : : "memory")
#define __dmb(opt)	asm volatile("dmb " #opt : : : "memory")
#define __dsb(opt)	asm volatile("dsb " #opt : : : "memory")

#define mb()		__dsb(sy)
#define rmb()		__dsb(ld)
#define wmb()		__dsb(st)

#define dma_rmb()	__dmb(oshld)
#define dma_wmb()	__dmb(oshst)

#define smp_mb()	__dmb(ish)
#define smp_rmb()	__dmb(ishld)
#define smp_wmb()	__dmb(ishst)

#endif
