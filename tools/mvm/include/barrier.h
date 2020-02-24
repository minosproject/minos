#ifndef __MVM_BARRIER_H__
#define __MVM_BARRIER_H__

#define isb()           asm volatile("isb" : : : "memory")
#define dmb(opt)        asm volatile("dmb " #opt : : : "memory")
#define dsb(opt)        asm volatile("dsb " #opt : : : "memory")

#define mb()            dsb(sy)
#define rmb()           dsb(ld)
#define wmb()           dsb(st)

#define dma_rmb()       dmb(oshld)
#define dma_wmb()       dmb(oshst)

#define smp_mb()        dmb(ish)
#define smp_rmb()       dmb(ishld)
#define smp_wmb()       dmb(ishst)

#endif
