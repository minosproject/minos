#include <mvisor/vcpu.h>
#include <mvisor/pcpu.h>
#include <mvisor/vm.h>
#include <mvisor/mvisor.h>
#include <mvisor/percpu.h>

static pcpu_t pcpus[CONFIG_NR_CPUS];
extern void switch_to_vcpu(pt_regs *regs);

extern unsigned char __percpu_start;
extern unsigned char __percpu_end;
extern unsigned char __percpu_section_size;

phy_addr_t percpu_offset[CONFIG_NR_CPUS];

DEFINE_PER_CPU(vcpu_t *, running_vcpu);
DEFINE_PER_CPU(pcpu_t *, pcpu);

void init_pcpus(void)
{
	int i;
	pcpu_t *pcpu;
	size_t size;

	size = (&__percpu_end) - (&__percpu_start);
	memset((char *)&__percpu_start, 0, size);

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		percpu_offset[i] = (phy_addr_t)(&__percpu_start) +
			(size_t)(&__percpu_section_size) * i;
	}

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		pcpu = &pcpus[i];
		init_list(&pcpu->vcpu_list);
		pcpu->pcpu_id = i;
		get_per_cpu(pcpu, i) = pcpu;
	}
}

uint32_t pcpu_affinity(vcpu_t *vcpu, uint32_t affinity)
{
	int i, found;
	uint32_t af;
	pcpu_t *pcpu;
	struct list_head *list;
	vcpu_t *tvcpu;

	/*
	 * first check the other vcpu belong to the same
	 * VM is areadly affinity to this pcpu
	 */
	if (affinity >= CONFIG_NR_CPUS)
		goto step2;

	pcpu = &pcpus[affinity];
	list_for_each(&pcpu->vcpu_list, list) {
		tvcpu = list_entry(list, vcpu_t, pcpu_list);
		if ((vcpu->vm_belong_to->vmid) ==
				(tvcpu->vm_belong_to->vmid))
			goto step2;
	}

	/*
	 * can affinity to this pcpu
	 */
	list_add_tail(&pcpu->vcpu_list, &vcpu->pcpu_list);
	vcpu->pcpu_affinity = affinity;
	return affinity;

step2:
	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		found = 0;
		if (i == affinity)
			continue;

		pcpu = &pcpus[i];
		list_for_each(&pcpu->vcpu_list, list) {
			tvcpu = list_entry(list, vcpu_t, pcpu_list);
			if ((vcpu->vm_belong_to->vmid) ==
					(tvcpu->vm_belong_to->vmid)) {
				found = 1;
				break;
			}
		}

		if (!found) {
			list_add_tail(&pcpu->vcpu_list, &vcpu->pcpu_list);
			vcpu->pcpu_affinity = i;
			return i;
		}
	}

	return PCPU_AFFINITY_FAIL;
}

static vcpu_t *find_vcpu_to_run(pcpu_t *pcpu)
{
	vcpu_t *vcpu;
	struct list_head *list;

	list_for_each(&pcpu->vcpu_list, list) {
		vcpu = list_entry(list, vcpu_t, pcpu_list);
		if (vmm_get_vcpu_state(vcpu) == VCPU_STATE_READY)
			return vcpu;
	}

	return NULL;
}

void sched_vcpu(void)
{
	int cpuid;
	pcpu_t *pcpu;
	struct list_head *list;
	vcpu_t *vcpu;

	cpuid = get_cpu_id();
	pcpu = get_cpu_var(pcpu);

	vcpu = find_vcpu_to_run(pcpu);
	if (vcpu == NULL) {
		/*
		 * can goto idle state
		 */
	} else {
		get_cpu_var(running_vcpu) = vcpu;
		//switch_to_vcpu(&vcpu->context);
		/*
		 * should never return here
		 */
	}
}
