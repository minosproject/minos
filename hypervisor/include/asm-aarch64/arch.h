#ifndef _MINOS_ARCH_AARCH64_H_
#define _MINOS_ARCH_AARCH64_H_

#include <minos/types.h>
#include <asm/aarch64_helper.h>

struct vcpu;

typedef struct aarch64_regs {
	uint64_t elr_elx;
	uint64_t spsr_elx;
	uint64_t esr_elx;
	uint64_t x0;
	uint64_t x1;
	uint64_t x2;
	uint64_t x3;
	uint64_t x4;
	uint64_t x5;
	uint64_t x6;
	uint64_t x7;
	uint64_t x8;
	uint64_t x9;
	uint64_t x10;
	uint64_t x11;
	uint64_t x12;
	uint64_t x13;
	uint64_t x14;
	uint64_t x15;
	uint64_t x16;
	uint64_t x17;
	uint64_t x18;
	uint64_t x19;
	uint64_t x20;
	uint64_t x21;
	uint64_t x22;
	uint64_t x23;
	uint64_t x24;
	uint64_t x25;
	uint64_t x26;
	uint64_t x27;
	uint64_t x28;
	uint64_t x29;
	uint64_t lr;
} gp_regs __align(sizeof(uint64_t));

#define NR_LOCAL_IRQS	(32)
#define SPI_OFFSET(n)	(n - NR_LOCAL_IRQS);
#define LOCAL_OFFSET(n) (n)

#define arch_disable_local_irq()	write_daifset(2)
#define arch_enable_local_irq() 	write_daifclr(2)

static inline unsigned long arch_save_irqflags(void)
{
	return	read_daif();
}

static inline void arch_restore_irqflags(unsigned long flags)
{
	write_daif(flags);
}

#define local_irq_save(flag) \
	do { \
		flag = arch_save_irqflags(); \
		arch_disable_local_irq(); \
	} while (0)

#define local_irq_restore(flag) \
	do { \
		arch_restore_irqflags(flag); \
	} while (0)


#define stack_to_gp_regs(base) \
	(gp_regs *)(base - sizeof(gp_regs))

#define get_reg_value(regs, index)	\
	*((unsigned long *)(regs) + index + 3)

#define set_reg_value(regs, index, value)	\
	*((unsigned long *)(regs) + index + 3) = (unsigned long)value

#define read_sysreg32(name) ({                          \
    uint32_t _r;                                        \
    asm volatile("mrs  %0, "stringify(name) : "=r" (_r));         \
    _r; })

#define write_sysreg32(v, name) do {                    \
    uint32_t _r = v;                                    \
    asm volatile("msr "stringify(name)", %0" : : "r" (_r));       \
} while (0)

#define write_sysreg64(v, name) do {                    \
    uint64_t _r = v;                                    \
    asm volatile("msr "stringify(name)", %0" : : "r" (_r));       \
} while (0)

#define read_sysreg64(name) ({                          \
    uint64_t _r;                                        \
    asm volatile("mrs  %0, "stringify(name) : "=r" (_r));         \
    _r; })

#define read_sysreg(name)     read_sysreg64(name)
#define write_sysreg(v, name) write_sysreg64(v, name)

static inline void flush_all_tlb_host(void)
{
	asm volatile (
		"dsb sy;"
		"tlbi alle2;"
		"dsb sy;"
		"isb;"
		: : : "memory"
	);
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

static inline void flush_local_tlbis_guest(void)
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

static inline void flush_all_tlb_guest(void)
{
	/* flush all vmids local TLBS, non-hypervisor mode */
	asm volatile(
		"dsb sy;"
		"tlbi alle1;"
		"dsb sy;"
		"isb;"
		: : : "memory"
	);
}

static inline void flush_all_tlbis_guest(void)
{
	/* flush innershareable TLBS, all VMIDs, non-hypervisor mode */
	asm volatile(
		"dsb sy;"
		"tlbi alle1is;"
		"dsb sy;"
		"isb;"
		: : : "memory"
	);
}

static inline unsigned long va_to_pa(unsigned long va)
{
	uint64_t pa, tmp = read_sysreg64(PAR_EL1);

	asm volatile ("at s1e2r, %0;" : : "r" (va));
	isb();
	pa = read_sysreg64(PAR_EL1) & 0x0000fffffffff000;
	pa = pa | (va & (~(~PAGE_MASK)));
	write_sysreg64(tmp, PAR_EL1);

	return pa;
}

static inline unsigned long guest_va_to_pa(unsigned long va, int read)
{
	uint64_t pa, tmp = read_sysreg64(PAR_EL1);

	if (read)
		asm volatile ("at s12e1r, %0;" : : "r" (va));
	else
		asm volatile ("at s12e1w, %0;" : : "r" (va));
	isb();
	pa = read_sysreg64(PAR_EL1) & 0x0000fffffffff000;
	pa = pa | (va & (~(~PAGE_MASK)));
	write_sysreg64(tmp, PAR_EL1);

	return pa;
}

static inline unsigned long guest_va_to_ipa(unsigned long va, int read)
{
	uint64_t pa, tmp = read_sysreg64(PAR_EL1);

	if (read)
		asm volatile ("at s1e1w, %0;" : : "r" (va));
	else
		asm volatile ("at s1e1r, %0;" : : "r" (va));
	isb();
	pa = read_sysreg64(PAR_EL1) & 0x0000fffffffff000;
	pa = pa | (va & (~(~PAGE_MASK)));
	write_sysreg64(tmp, PAR_EL1);

	return pa;
}

int arch_taken_from_guest(gp_regs *regs);
void arch_switch_vcpu_sw(void);
void arch_dump_stack(gp_regs *regs, unsigned long *sp);
unsigned long arch_get_fp(void);
unsigned long arch_get_lr(void);

#endif
