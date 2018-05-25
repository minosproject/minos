#include <minos/minos.h>
#include <virt/vmodule.h>

void exit_from_guest(struct vcpu *vcpu)
{
	do_hooks(vcpu, MINOS_HOOK_TYPE_EXIT_FROM_GUEST);
}

void enter_to_guest(struct vcpu *vcpu)
{
	do_hooks(vcpu, MINOS_HOOK_TYPE_ENTER_TO_GUEST);
}

static inline int is_vcpu_ready(struct vcpu *vcpu)
{
	return 0;
}

static inline int detach_ready_vcpu(struct vcpu *vcpu)
{
	return 0;
}

void vcpu_online(struct vcpu *vcpu)
{
}

void vcpu_offline(struct vcpu *vcpu)
{
}

int vcpu_power_on(struct vcpu *caller, int cpuid,
		unsigned long entry, unsigned long unsed)
{

	/*
	 * resched the pcpu since it may have in the
	 * wfi or wfe state, or need to sched the new
	 * vcpu as soon as possible
	 *
	 * vcpu belong the the same vm will not
	 * at the same pcpu
	 */

	return 0;
}

void sched_vcpu(struct vcpu *vcpu, int reason)
{
}

int vcpu_can_idle(struct vcpu *vcpu)
{
	return 0;
}

void vcpu_idle(struct vcpu *vcpu)
{
}

void switch_to_vcpu(struct vcpu *current, struct vcpu *next)
{

	/*
	 * if current != next and current != NULL
	 * then need to save the current cpu context
	 * to the current vcpu
	 * restore the next vcpu's context to the real
	 * hardware
	 */
}

#if 0

static int vcpu_need_to_run(struct vcpu *vcpu)
{
}

static void update_sched_info(struct vcpu *vcpu)
{
}

#endif

int reched_handler(uint32_t irq, void *data)
{
	return 0;
}

void hypervisor_init(void)
{
	vmodules_init();
	create_vms();
}
