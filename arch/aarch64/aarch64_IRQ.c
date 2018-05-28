#include <minos/minos.h>
#include <minos/irq.h>
#include <minos/softirq.h>
#include <virt/virt.h>
#include <minos/arch.h>

void irq_handler(gp_regs *regs)
{
	exit_from_guest(regs);

	/*
	 * keep irq disabled in EL2
	 */
	do_irq_handler();
	irq_exit();
}
