#ifndef __MINOS_ASM_TLB_H__
#define __MINOS_ASM_TLB_H__

#include <minos/vspace.h>

static inline void flush_tlb_asid_all(uint16_t asid)
{
	/*
	 * load the ttbr0_el0 value to the register
	 */
	asm volatile (
		"lsl %x0, %0, #48\n"
		"dsb sy\n"
		"tlbi aside1is, %x0\n"
		"dsb sy\n"
		"isb\n"
		:
		: "Ir" (asid)
		: "memory"
	);
}

static inline void flush_all_tlb_host(void)
{
#ifdef CONFIG_VIRT
	asm volatile (
		"dsb sy;"
		"tlbi alle2is;"
		"dsb sy;"
		"isb;"
		: : : "memory"
	);
#else
	asm volatile (
		"dsb sy;"
		"tlbi alle1is;"
		"dsb sy;"
		"isb;"
		: : : "memory"
	);
#endif
}

static inline void flush_local_tlb_host(void)
{
#ifdef CONFIG_VIRT
	asm volatile (
		"dsb sy;"
		"tlbi alle2;"
		"dsb sy;"
		"isb;"
		: : : "memory"
	);
#else
	asm volatile (
		"dsb sy;"
		"tlbi alle2;"
		"dsb sy;"
		"isb;"
		: : : "memory"
	);
#endif
}

static inline void flush_tlb_va_host(unsigned long va, unsigned long size)
{
	unsigned long end = va + size;

	dsb();

#ifdef CONFIG_VIRT
	while (va < end) {
		asm volatile("tlbi vae2is, %0;" : : "r"
				(va >> PAGE_SHIFT) : "memory");
		va += PAGE_SIZE;
	}
#else
	while (va < end) {
		asm volatile("tlbi vae1is, %0;" : : "r"
				(va >> PAGE_SHIFT) : "memory");
		va += PAGE_SIZE;
	}
#endif

	dsb();
	isb();
}

static inline void flush_local_tlb_va_host(unsigned long va, unsigned long size)
{
	unsigned long end = va + size;

	dsb();

#ifdef CONFIG_VIRT
	while (va < end) {
		asm volatile("tlbi vae2, %0;" : : "r"
				(va >> PAGE_SHIFT) : "memory");
		va += PAGE_SIZE;
	}
#else
	while (va < end) {
		asm volatile("tlbi vae2, %0;" : : "r"
				(va >> PAGE_SHIFT) : "memory");
		va += PAGE_SIZE;
	}
#endif

	dsb();
	isb();
}

static inline void flush_local_tlb_guest(void)
{
	/* current VMID only */
	asm volatile (
		"dsb sy;"
		"tlbi vmalls12e1;"
		"dsb sy;"
		"isb;"
		: : : "memory"
	);
}

static inline void flush_all_tlb_guest(void)
{
	/* current vmid only and innershareable TLBS */
	asm volatile(
		"dsb sy;"
		"tlbi vmalls12e1is;"
		"dsb sy;"
		"isb;"
		: : : "memory"
	);
}

static inline void flush_tlb_ipa_guest(unsigned long ipa, size_t size)
{
	unsigned long end = ipa + size;

	dsb();

	/*
	 * step 1 - flush stage2 tlb for va range
	 * step 2 - flush all stage1 tlb for this VM
	 */
	while (ipa < end) {
		asm volatile("tlbi ipas2e1is, %0;" : : "r"
				(ipa >> PAGE_SHIFT) : "memory");
		ipa += PAGE_SIZE;
	}

	asm volatile("tlbi vmalle1;");

	dsb();
	isb();
}

void flush_tlb_vm(struct vspace *mm);
void flush_tlb_vm_ipa_range(struct vspace *mm, unsigned long ipa, size_t size);

#endif
