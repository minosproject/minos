#ifndef _MVISOR_TIME_H_
#define _MVISOR_TIME_H_

#include <asm/time.h>

#define NOW()	get_sys_ticks()

unsigned long msecs_to_ticks(uint32_t ms);
unsigned long nsecs_to_ticks(uint32_t ns);

static inline void enable_timer(unsigned long e)
{
	arch_enable_timer(e);
}

#endif
