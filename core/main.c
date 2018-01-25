/*
 * Created by Le Min 2017/12/12
 */

#include <asm/cpu.h>
#include <core/core.h>

extern int init_mem_block(void);
extern void init_pcpus(void);
extern void init_vms(void);
extern int log_buffer_init(void);
extern void sched_vcpu(void);
extern void el2_stage2_vmsa_init(void);

int boot_main(void)
{
	int i;

	log_buffer_init();
	pr_info("Starting mVisor ...\n");

	if (get_cpu_id() != 0)
		panic("cpu is not cpu0");

	init_mem_block();
	el2_stage2_vmsa_init();
	init_pcpus();
	init_vms();
	//gic_global_init();
	//gic_local_init();
	vmm_irqs_init();

	/*
	 * now we can power on other cpu core
	 */
	for (i = 1; i < CONFIG_NUM_OF_CPUS; i++)
		power_on_cpu_core();

	sched_vcpu();

	return 0;
}

int boot_secondary(void)
{
	//gic_local_init();

	/*
	 * wait for boot cpu ready
	 */
	while (1) {
		if (get_cpu_id() != 1)
			pr_info("wait for boot cpu bootup %d\n", get_cpu_id());
	}

	sched_vcpu();
}
