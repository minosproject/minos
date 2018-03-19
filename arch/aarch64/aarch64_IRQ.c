#include <mvisor/mvisor.h>
#include <mvisor/irq.h>
#include <mvisor/sched.h>

void IRQ_from_el2_handler(void *data)
{
	vcpu_t *vcpu = get_cpu_var(current_vcpu);

	do_irq_handler(vcpu);
}

void IRQ_from_el1_handler(void *data)
{
	vmm_exit_from_guest((vcpu_t *)data);
	do_irq_handler((vcpu_t *)data);
	vmm_enter_to_guest((vcpu_t *)data);
}
