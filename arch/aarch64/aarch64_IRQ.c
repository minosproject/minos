#include <mvisor/irq.h>

void IRQ_from_el2_handler(void *data)
{
	do_irq_handler((vcpu_regs *)data);
}

void IRQ_from_el1_handler(void *data)
{
	do_irq_handler((vcpu_regs *)data);
}
