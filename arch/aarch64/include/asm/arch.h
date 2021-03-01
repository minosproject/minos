#ifndef _MINOS_ARCH_AARCH64_H_
#define _MINOS_ARCH_AARCH64_H_

#include <asm/gp_reg.h>
#include <asm/aarch64_helper.h>
#include <config/config.h>
#include <minos/task_def.h>

#ifdef CONFIG_VIRT
#include <asm/virt.h>
#endif

#define SP_SIZE		CONFIG_TASK_STACK_SIZE

#define NR_LOCAL_IRQS	(32)
#define NR_SGI_IRQS	(16)
#define NR_PPI_IRQS	(16)

#define SGI_IRQ_BASE	(0)
#define PPI_IRQ_BASE	(16)

#define SPI_OFFSET(n)	(n - NR_LOCAL_IRQS);
#define LOCAL_OFFSET(n) (n)

#define arch_disable_local_irq()	write_daifset(2)
#define arch_enable_local_irq() 	write_daifclr(2)

#define arch_save_irqflags()		read_daif()
#define arch_restore_irqflags(flags)	write_daif(flags)

#define arch_irq_disabled()		(read_daif() & (1 << DAIF_I_BIT))

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

static inline u64 get_reg_value(gp_regs *regs, int index)
{
	return (index == 31) ? 0 : *(&regs->x0 + index);
}

static inline void set_reg_value(gp_regs *regs, int index, u64 value)
{
	if (index == 31)
		return;

	*(&regs->x0 + index) = value;
}

#define read_sysreg32(name) ({						\
	uint32_t _r;							\
	asm volatile("mrs  %0, "stringify(name) : "=r" (_r));		\
	_r; })

#define write_sysreg32(v, name)						\
	do {								\
		uint32_t _r = v;					\
		asm volatile("msr "stringify(name)", %0" : : "r" (_r));	\
	} while (0)

#define write_sysreg64(v, name)						\
	do {								\
		uint64_t _r = v;					\
		asm volatile("msr "stringify(name)", %0" : : "r" (_r));	\
	} while (0)

#define read_sysreg64(name) ({						\
	uint64_t _r;							\
	asm volatile("mrs  %0, "stringify(name) : "=r" (_r));		\
	_r; })

#define read_sysreg(name)     read_sysreg64(name)
#define write_sysreg(v, name) write_sysreg64(v, name)

#define nop() asm ("nop")

static inline int affinity_to_cpuid(unsigned long affinity)
{
	int aff0, aff1;

#ifdef CONFIG_MPIDR_SHIFT
	aff0 = (affinity >> MPIDR_EL1_AFF1_LSB) & 0xff;
	aff1 = (affinity >> MPIDR_EL1_AFF2_LSB) & 0xff;
#else
	aff0 = (affinity >> MPIDR_EL1_AFF0_LSB) & 0xff;
	aff1 = (affinity >> MPIDR_EL1_AFF1_LSB) & 0xff;
#endif

	return (aff1 * CONFIG_NR_CPUS_CLUSTER0) + aff0;
}

static inline int affinity_to_logic_cpu(uint32_t aff3, uint32_t aff2,
		uint32_t aff1, uint32_t aff0)
{
	return (aff1 * CONFIG_NR_CPUS_CLUSTER0) + aff0;
}

static inline uint64_t cpuid_to_affinity(int cpuid)
{
	int aff0, aff1;

#ifdef CONFIG_MPIDR_SHIFT
	if (cpuid < CONFIG_NR_CPUS_CLUSTER0)
		return (cpuid << MPIDR_EL1_AFF1_LSB);
	else {
		aff0 = cpuid - CONFIG_NR_CPUS_CLUSTER0;
		aff1 = 1;

		return (aff1 << MPIDR_EL1_AFF2_LSB) |
				(aff0 << MPIDR_EL1_AFF1_LSB);
	}
#else
	if (cpuid < CONFIG_NR_CPUS_CLUSTER0) {
		return cpuid;
	} else {
		aff0 = cpuid - CONFIG_NR_CPUS_CLUSTER0;
		aff1 = 1;

		return (aff1 << MPIDR_EL1_AFF1_LSB) + aff0;
	}
#endif
}

#ifdef CONFIG_VIRT
register unsigned long __task_info asm ("sp");

static inline unsigned long current_sp(void)
{
	return ((__task_info + SP_SIZE) & ~(SP_SIZE - 1));
}

static inline struct task_info *current_task_info(void)
{
	return (struct task_info *)(current_sp() - sizeof(struct task_info));
}
#else
register unsigned long __task_info asm ("x28");
register unsigned long __current_sp asm ("sp");

static inline unsigned long current_sp(void)
{
	return __current_sp;
}

static inline struct task_info *current_task_info(void)
{
	return (struct task_info *)__task_info;
}
#endif

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

static inline void inv_icache_local(void)
{
	asm volatile("ic iallu");
	dsbsy();
	isb();
}

static inline void inv_icache_all(void)
{
	asm volatile("ic ialluis");
	dsbsy();
}

static inline int arch_smp_processor_id(void)
{
	uint64_t v;
	__asm__ volatile("mrs %0, TPIDR_EL2" : "=r" (v));
	return v;
}

void flush_dcache_range(unsigned long addr, size_t size);
void inv_dcache_range(unsigned long addr, size_t size);
void flush_cache_all(void);
void flush_dcache_all(void);
void inv_dcache_all(void);

int arch_taken_from_guest(gp_regs *regs);
void arch_switch_task_sw(void);
void arch_dump_stack(gp_regs *regs, unsigned long *sp);
void arch_dump_register(gp_regs *regs);
unsigned long arch_get_fp(void);
unsigned long arch_get_sp(void);
unsigned long arch_get_lr(void);
void arch_set_virq_flag(void);
void arch_set_vfiq_flag(void);
void arch_clear_vfiq_flag(void);
void arch_clear_virq_flag(void);
void arch_smp_init(phy_addr_t *smp_h_addr);
int __arch_init(void);
int arch_early_init(void);
void arch_init_task(struct task *task, void *entry, void *data);
void arch_release_task(struct task *task);

#endif
