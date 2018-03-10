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

void boot_main(void)
{
	int i;

	vmm_early_init();
	vmm_mm_init();

	vmm_log_init();

	pr_info("Starting mVisor ...\n");

	if (get_cpu_id() != 0)
		panic("cpu is not cpu0");

	vmm_arch_init();
	vmm_percpus_init();
	vmm_pcpus_init();
	vmm_smp_init();
	vmm_irq_init();
	vmm_mmu_init();
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

	/*
	 * wake up other cpus
	 */
	smp_cpus_up();
	enable_local_irq();

	sched_vcpu();
}

void boot_secondary(void)
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

	vmm_irq_secondary_init();
	enable_local_irq();
	smp_holding_pen[cpuid] = 0xffff;
	//gic_send_sgi(15, SGI_TO_SELF, NULL);

	sched_vcpu();
}
