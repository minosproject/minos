#include <mvisor/mvisor.h>
#include <mvisor/irq.h>

void IRQ_from_el2_handler(void *data)
{
	do_irq_handler();
}

void IRQ_from_el1_handler(void *data)
{
	vmm_exit_from_guest((vcpu_t *)data);
	do_irq_handler();
}
