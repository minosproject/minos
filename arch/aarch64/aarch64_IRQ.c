#include <mvisor/mvisor.h>
#include <mvisor/irq.h>

void IRQ_from_el2_handler(void *data)
{
	vcpu_t *vcpu = get_per_cpu(running_vcpu);

	do_irq_handler(vcpu);
}

void IRQ_from_el1_handler(void *data)
{
	vmm_exit_from_guest((vcpu_t *)data);

	do_irq_handler((vcpu_regs *)data);

	vmm_resume_to_guest((vcpu_t *)data);
}
