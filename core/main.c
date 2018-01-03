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
	log_buffer_init();
	pr_info("Starting mVisor ...\n");

	if (get_cpu_id() != 0)
		panic("cpu is not cpu0");

	init_mem_block();
	el2_stage2_vmsa_init();
	init_pcpus();
	init_vms();

	//wake_up_other_cpus();

	sched_vcpu();

	return 0;
}

int boot_secondary(void)
{
	sched_vcpu();
}
