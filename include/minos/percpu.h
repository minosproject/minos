#ifndef _MINOS_PERCPU_H_
#define _MINOS_PERCPU_H_

#include <config/config.h>
#include <minos/types.h>
#include <minos/arch.h>
#include <minos/preempt.h>

extern unsigned long percpu_offset[];

void percpus_init(void);

#define DEFINE_PER_CPU(type, name) \
	__section(".__percpu") __typeof__(type) per_cpu_##name

#define DECLARE_PER_CPU(type, name) \
	extern __section(".__percpu") __typeof__(type) per_cpu_##name

#define get_per_cpu(name, cpu) \
	(*((__typeof__(per_cpu_##name)*)((unsigned char*)&per_cpu_##name - percpu_offset[0] + percpu_offset[cpu])))

#define get_cpu_var(name) get_per_cpu(name, smp_processor_id())

#define get_cpu_data(name)	\
({				\
	preempt_disable();	\
	get_cpu_var(name);	\
})

#define put_cpu_data(name)	\
({				\
	preempt_enable();	\
})

#endif
