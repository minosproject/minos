#ifndef _MINOS_PERCPU_H_
#define _MINOS_PERCPU_H_

#include <config/config.h>
#include <minos/types.h>
#include <minos/arch.h>
#include <minos/preempt.h>
#include <minos/flag.h>

#define PCPU_IDLE_F_TASKS_RELEASE	(1 << 0)

typedef enum {
	PCPU_STATE_OFFLINE	= 0x0,
	PCPU_STATE_RUNNING,
	PCPU_STATE_IDLE,
} pcpu_state_t;

struct task;

struct pcpu {
	int pcpu_id;
	volatile int state;

	unsigned long percpu_offset;

	uint32_t nr_pcpu_task;

	int os_is_running;

	spinlock_t lock;
	struct list_head new_list;
	struct list_head stop_list;

	struct list_head die_process;

	struct task *running_task;	// current task
	struct task *idle_task;

	/*
	 * each pcpu has its local sched list, 8 priority
	 * local_rdy_grp only use [0 - 8], in these 8
	 * priority:
	 * 7 - used for idle task
	 * 6 - used for vcpu task
	 */
	uint8_t local_rdy_grp;
	struct list_head ready_list[8];

	/*
	 * each pcpu will have one kernel task which
	 * will do some maintenance work for the pcpu
	 */
	struct task *kworker;
	struct flag_grp fg;

	void *stack;

} __cache_line_align;

extern unsigned long percpu_offset[];

void percpu_init(int cpuid);

static inline int smp_processor_id(void)
{
	return ((struct pcpu *)arch_get_pcpu_data())->pcpu_id;
}

#define DEFINE_PER_CPU(type, name) \
	__section(".__percpu") __typeof__(type) per_cpu_##name

#define DECLARE_PER_CPU(type, name) \
	extern __section(".__percpu") __typeof__(type) per_cpu_##name

#define get_per_cpu(name, cpu) \
	(*((__typeof__(per_cpu_##name)*)((unsigned char*)&per_cpu_##name - percpu_offset[0] + percpu_offset[cpu])))

#define get_cpu_var(name) get_per_cpu(name, smp_processor_id())

#define get_pcpu()	((struct pcpu *)arch_get_pcpu_data())

#define __get_pcpu()		\
({				\
	preempt_disable();	\
	((struct pcpu *)arch_get_pcpu_data());	\
})

#define __put_pcpu(pcpu)	\
({				\
	preempt_enable();	\
 })

#define get_cpu_data(name)	\
({				\
	preempt_disable();	\
	get_cpu_var(name);	\
})

#define put_cpu_data(name)	\
({				\
	preempt_enable();	\
})

#define get_cpu()		\
({				\
	preempt_disable();	\
	smp_processor_id();	\
})				\

#define put_cpu()		\
({				\
	preempt_enable();	\
})

#endif
