#ifndef _MVISOR_SMP_H_
#define _MVISOR_SMP_H_

#include <mvisor/types.h>
#include <mvisor/percpu.h>
#include <mvisor/bitmap.h>
#include <config/config.h>

#define NR_CPUS		CONFIG_NR_CPUS

#define MPIDR_ID_MASK	(0x000000ff00ffffff)

DECLARE_PER_CPU(uint64_t, cpu_id);
extern uint64_t *smp_holding_pen;

int get_cpu_id(void);
void smp_cpus_up(void);
int smp_cpu_up(uint64_t mpidr_id);

void vmm_smp_init(void);

#define smp_processor_id()	get_cpu_id()
#define for_all_cpu(cpu)	\
	for (cpu = 0; cpu < NR_CPUS; cpu++)

static inline int affinity_to_logic_cpu(uint32_t aff3, uint32_t aff2,
		uint32_t aff1, uint32_t aff0)
{
	return aff0;
}

#endif
