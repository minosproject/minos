#ifndef __MINOS_ASM_CURRENT_H__
#define __MINOS_ASM_CURRENT_H__

#include <config/config.h>
#include <minos/compiler.h>
#include <asm/barrier.h>

static inline void *asm_get_current_task(void)
{
	void *tsk;
	__asm__ volatile ("mov %0, x18" : "=r" (tsk));
	return tsk;
}

static inline void *asm_get_current_task_info(void)
{
	void *tsk_info;
	__asm__ volatile ("mov %0, x18" : "=r" (tsk_info));
	return tsk_info;
}

static inline void asm_set_current_task(void *task)
{
	__asm__ volatile ("mov x18, %0" : : "r" (task));
}

#ifdef CONFIG_VIRT
static inline void arch_set_pcpu_data(void *pcpu)
{
	__asm__ volatile("msr TPIDR_EL2, %0" : : "r" (pcpu));
}

static inline void *arch_get_pcpu_data(void)
{
	uint64_t v;
	__asm__ volatile("mrs %0, TPIDR_EL2" : "=r" (v));
	return (void *)v;
}

static inline int __smp_processor_id(void)
{
	uint64_t v;
	int cpu;

	__asm__ volatile (
		"mrs	%0, TPIDR_EL2\n"
		"ldrh	%w1, [%0, #0]\n"
		: "=r" (v), "=r" (cpu)
		:
		: "memory"
	);

	return cpu;
}
#else
static inline void arch_set_pcpu_data(void *pcpu)
{
	__asm__ volatile("msr TPIDR_EL1, %0" : : "r" (pcpu));
}

static inline void *arch_get_pcpu_data(void)
{
	uint64_t v;
	__asm__ volatile("mrs %0, TPIDR_EL1" : "=r" (v));
	return (void *)v;
}

static inline int __smp_processor_id(void)
{
	uint64_t v;
	int cpu;

	__asm__ volatile (
		"mrs	%0, TPIDR_EL1\n"
		"ldrh	%w1, [%0, #0]\n"
		: "=r" (v), "=r" (cpu)
		:
		: "memory"
	);

	return cpu;
}
#endif

#endif
