#ifndef __MINOS_PREEMPT_H__
#define __MINOS_PREEMPT_H__

#include <minos/percpu.h>
#include <minos/atomic.h>

DECLARE_PER_CPU(int, __preempt);

#define preempt_disable()	get_cpu_var(__preempt)++
#define preempt_enable()	get_cpu_var(__preempt)--
#define preempt_allowed()	(!get_cpu_var(__preempt))

#endif
