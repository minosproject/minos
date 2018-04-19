#include <mvisor/mvisor.h>
#include <mvisor/irq.h>
#include <mvisor/softirq.h>

void IRQ_from_el2_handler(void *data)
{
	do_irq_handler();
	irq_exit();
}

void IRQ_from_el1_handler(void *data)
{
	vmm_exit_from_guest((struct vcpu *)data);

	/*
	 * keep irq disabled in EL2
	 */
	do_irq_handler();
	irq_exit();
}
