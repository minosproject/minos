#include <mvisor/mvisor.h>

void arch_enable_timer(unsigned long expires)
{

}

unsigned long get_sys_ticks()
{

}

static int timers_init(void)
{

}

subsys_initcall_percpu(timers_init);
