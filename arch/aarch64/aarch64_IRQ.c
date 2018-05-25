#include <minos/minos.h>
#include <minos/irq.h>
#include <minos/softirq.h>
#include <virt/virt.h>

void IRQ_from_el2_handler(void *data)
{
	do_irq_handler();
	irq_exit();
}

void IRQ_from_el1_handler(void *data)
{
	exit_from_guest((struct vcpu *)data);

	/*
	 * keep irq disabled in EL2
	 */
	do_irq_handler();
	irq_exit();
}
