/*
 * Created by Le Min 2017/12/12
 */

#include <mvisor/mvisor.h>
#include <mvisor/percpu.h>
#include <mvisor/irq.h>
#include <mvisor/mm.h>
#include <asm/arch.h>
#include <mvisor/vcpu.h>
#include <mvisor/pm.h>
#include <mvisor/init.h>
#include <mvisor/sched.h>
#include <mvisor/smp.h>
#include <mvisor/atomic.h>

extern void softirq_init(void);
extern void init_timers(void);
extern int mvisor_modules_init(void);

extern struct mvisor_config mvisor_config;
struct mvisor_config *mv_config = &mvisor_config;

struct list_head hook_lists[MVISOR_HOOK_TYPE_UNKNOWN];

static void mvisor_hook_init(void)
{
	int i;

	for (i = 0; i < MVISOR_HOOK_TYPE_UNKNOWN; i++)
		init_list(&hook_lists[i]);
}

int mvisor_register_hook(hook_func_t fn, void *data, enum mvisor_hook_type type)
{
	struct mvisor_hook *hook;

	if ((fn == NULL) || (type >= MVISOR_HOOK_TYPE_UNKNOWN)) {
		pr_error("Hook info is invaild\n");
		return -EINVAL;
	}

	hook = (struct mvisor_hook *)mvisor_malloc(sizeof(struct mvisor_hook));
	if (!hook)
		return -ENOMEM;

	memset((char *)hook, 0, sizeof(struct mvisor_hook));
	hook->fn = fn;
	hook->data = data;
	init_list(&hook->list);

	list_add_tail(&hook_lists[type], &hook->list);

	return 0;
}

static int mvisor_do_hooks(struct vcpu *vcpu, enum mvisor_hook_type type)
{
	struct mvisor_hook *hook;

	list_for_each_entry(hook, &hook_lists[type], list)
		hook->fn(vcpu, hook->data);

	return 0;
}

void mvisor_exit_from_guest(struct vcpu *vcpu)
{
	mvisor_do_hooks(vcpu, MVISOR_HOOK_TYPE_EXIT_FROM_GUEST);
}

void mvisor_enter_to_guest(struct vcpu *vcpu)
{
	mvisor_do_hooks(vcpu, MVISOR_HOOK_TYPE_ENTER_TO_GUEST);
}

static void mvisor_parse_irqtags(void)
{
	int i;
	size_t size = mv_config->nr_irqtag;
	struct mvisor_irqtag *irqtags = mv_config->irqtags;

	for (i = 0; i < size; i++)
		mvisor_register_irq_entry(&irqtags[i]);
}

static void mvisor_parse_memtags(void)
{
	int i;
	size_t size = mv_config->nr_memtag;
	struct mvisor_memtag *memtags = mv_config->memtags;

	for (i = 0; i < size; i++)
		mvisor_register_memory_region(&memtags[i]);

}

void mvisor_parse_resource(void)
{
	mvisor_parse_irqtags();
	mvisor_parse_memtags();
}

void boot_main(void)
{
	/*
	 * need first init the mem alloctor
	 */
	mvisor_log_init();
	mvisor_mm_init();
	mvisor_modules_init();

	mvisor_mmu_init();

	mvisor_early_init();
	mvisor_early_init_percpu();

	mvisor_percpus_init();

	pr_info("Starting mVisor ...\n");

	mvisor_hook_init();

	if (get_cpu_id() != 0)
		panic("cpu is not cpu0");

	mvisor_arch_init();
	mvisor_arch_init_percpu();

	mvisor_pcpus_init();
	mvisor_smp_init();
	mvisor_irq_init();
	softirq_init();
	init_timers();
	mvisor_create_vms();

	/*
	 * here we need to handle all the resource
	 * config include memory and irq
	 */
	mvisor_parse_resource();
	mvisor_setup_irqs();

	/*
	 * prepare each vm to run
	 */
	mvisor_vms_init();

	mvisor_subsys_init();
	mvisor_subsys_init_percpu();

	/*
	 * wake up other cpus
	 */
	smp_cpus_up();
	enable_local_irq();

	mvisor_device_init();
	mvisor_device_init_percpu();

	mvisor_boot_vms();

	sched();
}

void boot_secondary(void)
{
	uint32_t cpuid = get_cpu_id();

	pr_info("cpu-%d is up\n", cpuid);

	mvisor_early_init_percpu();

	mvisor_arch_init_percpu();

	irq_desc_secondary_init();

	mvisor_subsys_init_percpu();

	enable_local_irq();

	mvisor_device_init_percpu();

	sched();
}

