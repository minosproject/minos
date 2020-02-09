#ifndef __ASM_TLB_H__
#define __ASM_TLB_H__

static inline void arch_flush_tlb_host(void)
{
	asm volatile (
		"dsb sy;"
		"tlbi alle2is;"
		"dsb sy;"
		"isb;"
		: : : "memory"
	);
}

static inline void arch_flush_local_tlb_host(void)
{
	asm volatile (
		"dsb sy;"
		"tlbi alle2;"
		"dsb sy;"
		"isb;"
		: : : "memory"
	);
}

static inline void arch_flush_local_tlb_guest(void)
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

static inline void arch_flush_tlb_guest(void)
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

static inline void arch_flush_tlb_ipa_guest(unsigned long ipa, size_t size)
{
	unsigned long end = ipa + size;

	dsb();

	while (ipa < end) {
		asm volatile("tlbi ipas2e1is, %0;" : : "r"
				(ipa >> PAGE_SHIFT) : "memory");
		ipa += PAGE_SIZE;
	}

	asm volatile("tlbi vmalle1;");

	dsb();
	isb();
}

static inline void arch_flush_tlb_va_host(unsigned long va,
		unsigned long size)
{
	unsigned long end = va + size;

	dsb();

	while (va < end) {
		asm volatile("tlbi vae2is, %0;" : : "r"
				(va >> PAGE_SHIFT) : "memory");
		va += PAGE_SIZE;
	}

	dsb();
	isb();
}

static inline void arch_flush_local_tlb_va_host(unsigned long va,
		unsigned long size)
{
	unsigned long end = va + size;

	dsb();

	while (va < end) {
		asm volatile("tlbi vae2, %0;" : : "r"
				(va >> PAGE_SHIFT) : "memory");
		va += PAGE_SIZE;
	}

	dsb();
	isb();
}

#endif
