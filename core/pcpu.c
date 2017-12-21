#include <core/vcpu.h>
#include <core/pcpu.h>
#include <core/vm.h>
#include <asm/cpu.h>

static pcpu_t pcpus[MAX_CPU_NR];
extern void switch_to_vcpu(vcpu_context_t *context);

void init_pcpus(void)
{
	int i;
	pcpu_t *pcpu;

	for (i = 0; i < MAX_CPU_NR; i++) {
		pcpu = &pcpus[i];
		init_list(&pcpu->vcpu_list);
		pcpu->pcpu_id = i;
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
	if (affinity >= MAX_CPU_NR)
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
	for (i = 0; i < MAX_CPU_NR; i++) {
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
		if (get_vcpu_state(vcpu) == VCPU_STATE_READY)
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
	pcpu = &pcpus[cpuid];

	vcpu = find_vcpu_to_run(pcpu);
	if (vcpu == NULL) {
		/*
		 * can goto idle state
		 */
	} else {
		switch_to_vcpu(&vcpu->context);
		/*
		 * should never return here
		 */
	}
}
