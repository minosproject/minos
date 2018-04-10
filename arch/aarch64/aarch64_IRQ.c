#include <mvisor/mvisor.h>
#include <mvisor/irq.h>

void IRQ_from_el2_handler(void *data)
{
	do_irq_handler();
}

void IRQ_from_el1_handler(void *data)
{
	pr_info("test");
	vmm_exit_from_guest((struct vcpu *)data);
	do_irq_handler();
}
