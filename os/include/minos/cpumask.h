#ifndef _MINOS_CPUMASK_H_
#define _MINOS_CPUMASK_H_

#include <minos/types.h>
#include <minos/bitmap.h>

typedef struct cpumask {
	 DECLARE_BITMAP(bits, CONFIG_NR_CPUS);
} cpumask_t;

#define nr_cpumask_bits (BITS_TO_LONGS(CONFIG_NR_CPUS) * BITS_PER_LONG)
#define nr_cpu_ids CONFIG_NR_CPUS

/* verify cpu argument to cpumask_* operators */
static inline unsigned int cpumask_check(unsigned int cpu)
{
	/*
	 * do some check
	 */
	return cpu;
}

static inline void cpumask_set_cpu(int cpu, cpumask_t *dstp)
{
	set_bit(cpumask_check(cpu), dstp->bits);
}

static inline void __cpumask_set_cpu(int cpu, cpumask_t *dstp)
{
	set_bit(cpumask_check(cpu), dstp->bits);
}

static inline void cpumask_clear_cpu(int cpu, cpumask_t *dstp)
{
	clear_bit(cpumask_check(cpu), dstp->bits);
}

static inline void __cpumask_clear_cpu(int cpu, cpumask_t *dstp)
{
	clear_bit(cpumask_check(cpu), dstp->bits);
}

static inline void cpumask_setall(cpumask_t *dstp)
{
	bitmap_fill(dstp->bits, nr_cpumask_bits);
}

static inline void cpumask_clear(cpumask_t *dstp)
{
	bitmap_zero(dstp->bits, nr_cpumask_bits);
}

static inline int cpumask_first(const cpumask_t *srcp)
{
	return min(nr_cpu_ids, find_first_bit(srcp->bits, nr_cpu_ids));
}

static inline int cpumask_next(int n, const cpumask_t *srcp)
{
	/* -1 is a legal arg here. */
	if (n != -1)
		cpumask_check(n);

	return min(nr_cpu_ids, find_next_bit(srcp->bits, nr_cpu_ids, n + 1));
}


#if CONFIG_NR_CPUS > 1
#define for_each_cpu(cpu, mask)			\
	for ((cpu) = cpumask_first(mask);	\
	     (cpu) < CONFIG_NR_CPUS;		\
	     (cpu) = cpumask_next(cpu, mask))
#else /* CONFIG_NR_CPUS == 1 */
#define for_each_cpu(cpu, mask)			\
	for ((cpu) = 0; (cpu) < 1; (cpu)++, (void)(mask))
#endif /* CONFIG_NR_CPUS */

#endif
