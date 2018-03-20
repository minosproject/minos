#include <mvisor/mvisor.h>
#include <asm/arch.h>

void cpu_idle(void)
{
	while (1) {
		wfi();
	}
}
