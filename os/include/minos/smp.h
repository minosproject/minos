#ifndef _MINOS_SMP_H_
#define _MINOS_SMP_H_

#include <minos/types.h>
#include <minos/percpu.h>
#include <minos/bitmap.h>
#include <config/config.h>
#include <minos/cpumask.h>

#define NR_CPUS		CONFIG_NR_CPUS

#define for_all_cpu(cpu)	\
	for (cpu = 0; cpu < NR_CPUS; cpu++)

extern cpumask_t cpu_online;

#define for_each_online_cpu(cpu) for_each_cpu(cpu, &cpu_online)

typedef void (*smp_function)(void *);

void smp_cpus_up(void);
void smp_init(void);
int smp_function_call(int cpu, smp_function fn,
		void *data, int wait);

#endif
