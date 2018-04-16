#ifndef _MVISOR_ASM_TIME_H_
#define _MVISOR_ASM_TIME_H_

unsigned long get_sys_ticks();
void arch_enable_timer(unsigned long e);

#endif
