#ifndef __MINOS_TLB_H__
#define __MINOS_TLB_H__

#include <asm/tlb.h>

#define flush_tlb_host()			arch_flush_tlb_host()
#define flush_local_tlb_host()			arch_flush_local_tlb_host()
#define flush_tlb_va_host(va, size)		arch_flush_tlb_va_host(va, size)
#define flush_local_tlb_va_host(va, size)	arch_flush_local_tlb_va_host(va, size)

#define flush_local_tlb_guest()			arch_flush_local_tlb_guest()
#define flush_tlb_guest()			arch_flush_tlb_guest()
#define flush_tlb_ipa_guest(ipa, size)		arch_flush_tlb_ipa_guest(ipa, size)

#endif
