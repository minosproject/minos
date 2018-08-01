#ifndef __MINOS_PREEMPT_H__
#define __MINOS_PREEMPT_H__

#include <minos/percpu.h>
#include <minos/atomic.h>

DECLARE_PER_CPU(atomic_t, preempt);

#define preempt_disable()	atomic_inc(&get_cpu_var(preempt))
#define preempt_enable()	atomic_dec(&get_cpu_var(preempt))
#define preempt_allowed()	(!atomic_read(&get_cpu_var(preempt)))

#endif
