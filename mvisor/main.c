/*
 * Created by Le Min 2017/12/12
 */

#include <mvisor/mvisor.h>
#include <mvisor/percpu.h>
#include <mvisor/irq.h>
#include <mvisor/mm.h>
#include <asm/arch.h>

extern void init_pcpus(void);
extern void init_vms(void);
extern int log_buffer_init(void);
extern void sched_vcpu(void);
extern void el2_stage2_vmsa_init(void);
extern void vmm_irqs_init(void);
extern void smp_init(void);

int boot_main(void)
{
	int i;

	log_buffer_init();
	pr_info("Starting mVisor ...\n");

	if (get_cpu_id() != 0)
		panic("cpu is not cpu0");

	arch_early_init();
	vmm_mm_init();
	arch_init();
	//el2_stage2_vmsa_init();
	init_pcpus();
	smp_init();
	init_vms();
	//gic_init();
	//vmm_irqs_init();

	/*
	 * wake up other cpus
	 */
	smp_cpus_up();
	//enable_irq();
	//wait_cpus_up();
	//gic_send_sgi(15, SGI_TO_SELF, NULL);

	sched_vcpu();

	return 0;
}

int boot_secondary(void)
{
	uint32_t cpuid = get_cpu_id();
	uint64_t mid;
	uint64_t mpidr;

	/*
	 * here wait up bootup cpu to wakeup us
	 */
	mpidr = read_mpidr_el1() & MPIDR_ID_MASK;
	for (;;) {
		mid = smp_holding_pen[cpuid];
		if (mpidr == mid)
			break;
	}

	get_per_cpu(cpu_id, cpuid) = mpidr;
	pr_info("cpu-%d is up\n", cpuid);

	//gic_secondary_init();
	//enable_irq();
	smp_holding_pen[cpuid] = 0xffff;
	//gic_send_sgi(15, SGI_TO_SELF, NULL);

	while (1);

	sched_vcpu();
}
