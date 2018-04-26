/*
 * Created by Le Min 2017/12/12
 */

#include <mvisor/mvisor.h>
#include <mvisor/percpu.h>
#include <mvisor/irq.h>
#include <mvisor/mm.h>
#include <asm/arch.h>
#include <mvisor/vcpu.h>
#include <mvisor/resource.h>
#include <mvisor/pm.h>
#include <mvisor/init.h>
#include <mvisor/sched.h>
#include <mvisor/smp.h>

extern void softirq_init(void);
extern void init_timers(void);
extern int vmm_modules_init(void);

struct list_head hook_lists[VMM_HOOK_TYPE_UNKNOWN];

static void vmm_hook_init(void)
{
	int i;

	for (i = 0; i < VMM_HOOK_TYPE_UNKNOWN; i++)
		init_list(&hook_lists[i]);
}

int vmm_register_hook(hook_func_t fn, void *data, enum vmm_hook_type type)
{
	struct vmm_hook *hook;

	if ((fn == NULL) || (type >= VMM_HOOK_TYPE_UNKNOWN)) {
		pr_error("Hook info is invaild\n");
		return -EINVAL;
	}

	hook = (struct vmm_hook *)vmm_malloc(sizeof(struct vmm_hook));
	if (!hook)
		return -ENOMEM;

	memset((char *)hook, 0, sizeof(struct vmm_hook));
	hook->fn = fn;
	hook->data = data;
	init_list(&hook->list);

	list_add_tail(&hook_lists[type], &hook->list);

	return 0;
}

static int vmm_do_hooks(struct vcpu *vcpu, enum vmm_hook_type type)
{
	struct vmm_hook *hook;

	list_for_each_entry(hook, &hook_lists[type], list)
		hook->fn(vcpu, hook->data);

	return 0;
}

void vmm_exit_from_guest(struct vcpu *vcpu)
{
	vmm_do_hooks(vcpu, VMM_HOOK_TYPE_EXIT_FROM_GUEST);
}

void vmm_enter_to_guest(struct vcpu *vcpu)
{
	vmm_do_hooks(vcpu, VMM_HOOK_TYPE_ENTER_TO_GUEST);
}

void boot_main(void)
{
	/*
	 * need first init the mem alloctor
	 */
	vmm_log_init();
	vmm_mm_init();
	vmm_modules_init();

	vmm_mmu_init();

	vmm_early_init();
	vmm_early_init_percpu();

	vmm_percpus_init();

	pr_info("Starting mVisor ...\n");

	vmm_hook_init();

	if (get_cpu_id() != 0)
		panic("cpu is not cpu0");

	vmm_arch_init();
	vmm_arch_init_percpu();

	vmm_pcpus_init();
	vmm_smp_init();
	vmm_irq_init();
	softirq_init();
	init_timers();
	vmm_create_vms();

	/*
	 * here we need to handle all the resource
	 * config include memory and irq
	 */
	vmm_parse_resource();
	vmm_setup_irqs();

	/*
	 * prepare each vm to run
	 */
	vmm_vms_init();

	vmm_subsys_init();
	vmm_subsys_init_percpu();

	/*
	 * wake up other cpus
	 */
	smp_cpus_up();
	enable_local_irq();

	vmm_device_init();
	vmm_device_init_percpu();


	sched();
}

void boot_secondary(void)
{
	uint32_t cpuid = get_cpu_id();
	uint64_t mid;

	pr_info("cpu-%d is up\n", cpuid);

	vmm_early_init_percpu();

	vmm_arch_init_percpu();

	vmm_irq_secondary_init();

	vmm_subsys_init_percpu();

	enable_local_irq();

	vmm_device_init_percpu();

	sched();
}

