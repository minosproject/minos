#ifndef __MINOS_OS_H__
#define __MINOS_OS_H__

#include <minos/percpu.h>
#include <minos/atomic.h>
#include <asm/arch.h>
#include <minos/bitops.h>
#include <minos/task_def.h>

static inline int os_is_running(void)
{
	return get_pcpu()->os_is_running;
}

static inline void set_os_running(void)
{
	/* 
	 * os running is set before the irq is enable
	 * so do not need to aquire lock or disable the
	 * interrupt here
	 */
	get_pcpu()->os_is_running = 1;
	wmb();
}

#endif
