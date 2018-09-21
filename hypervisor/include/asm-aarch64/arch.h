#ifndef _MINOS_ARCH_AARCH64_H_
#define _MINOS_ARCH_AARCH64_H_

#include <minos/types.h>
#include <asm/aarch64_helper.h>
#include <config/config.h>

struct vcpu;
struct vm;

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
#define NR_SGI_IRQS	(16)
#define NR_PPI_IRQS	(16)

#define SGI_IRQ_BASE	(0)
#define PPI_IRQ_BASE	(16)

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

static inline int affinity_to_vcpuid(unsigned long affinity)
{
	int aff0, aff1;

#define VM_NR_CPUS_CLUSTER	256
	aff1 = (affinity >> 8) & 0xff;
	aff0 = affinity & 0xff;

	return (aff1 * VM_NR_CPUS_CLUSTER) + aff0;
}

static inline int affinity_to_logic_cpu(uint32_t aff3, uint32_t aff2,
		uint32_t aff1, uint32_t aff0)
{
	return (aff1 * CONFIG_NR_CPUS_CLUSTER0) + aff0;
}

static inline uint64_t cpuid_to_affinity(int cpuid)
{
	int aff0, aff1;

	if (cpuid < CONFIG_NR_CPUS_CLUSTER0)
		return cpuid;
	else {
		aff0 = cpuid - CONFIG_NR_CPUS_CLUSTER0;
		aff1 = 1;

		return (aff1 << 8) + aff0;
	}
}

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

static inline void flush_tlb_va_host(unsigned long va,
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

static inline void cpu_relax(void)
{
	asm volatile("yield" ::: "memory");
}

extern unsigned long smc_call(uint32_t id, unsigned long a1,
		unsigned long a2, unsigned long a3, unsigned long a4,
		unsigned long a5, unsigned long a6);

void dcsw_op_louis(uint32_t op_type);
void dcsw_op_all(uint32_t op_type);
void flush_dcache_range(unsigned long addr, size_t size);
void clean_dcache_range(unsigned long addr, size_t size);
void inv_dcache_range(unsigned long addr, size_t size);
void flush_cache_all(void);
void flush_dcache_all(void);

int arch_taken_from_guest(gp_regs *regs);
void arch_switch_vcpu_sw(void);
void arch_dump_stack(gp_regs *regs, unsigned long *sp);
unsigned long arch_get_fp(void);
unsigned long arch_get_lr(void);
void arch_hvm_init(struct vm *vm);

#endif
