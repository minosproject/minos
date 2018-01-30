#ifndef _MVISOR_SMP_H_
#define _MVISOR_SMP_H_

#include <core/types.h>
#include <core/percpu.h>

#define MPIDR_ID_MASK	(0x000000ff00ffffff)

DECLARE_PER_CPU(uint64_t, cpu_id);
extern uint64_t smp_holding_pen[];

int get_cpu_id(void);
void smp_cpus_up(void);
int smp_cpu_up(uint64_t mpidr_id);

static inline uint64_t
generate_vcpu_id(uint32_t pcpu_id, uint32_t vm_id, uint32_t vcpu_id)
{
	return ((vcpu_id) | (vm_id << 8) | (pcpu_id << 16));
}

#endif
