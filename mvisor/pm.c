#include <mvisor/mvisor.h>
#include <asm/arch.h>
#include <mvisor/sched.h>

void cpu_idle(void)
{
	struct pcpu *pcpu = get_cpu_var(pcpu);

	/*
	 * if interrupt is happend when after the
	 * pcpu state is setted ? TBD
	 */
	pcpu->state = PCPU_STATE_IDLE;
	wfi();
	pcpu->state = PCPU_STATE_RUNNING;
}
