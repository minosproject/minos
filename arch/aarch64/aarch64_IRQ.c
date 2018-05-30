#include <minos/minos.h>
#include <minos/irq.h>
#include <minos/softirq.h>
#include <virt/virt.h>
#include <minos/arch.h>

void irq_c_handler(gp_regs *regs)
{
	irq_enter(regs);

	do_irq_handler();

	irq_exit(regs);
}
