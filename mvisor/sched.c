#include <mvisor/vcpu.h>
#include <mvisor/sched.h>
#include <mvisor/vm.h>
#include <mvisor/mvisor.h>
#include <mvisor/percpu.h>
#include <mvisor/pm.h>

static pcpu_t pcpus[CONFIG_NR_CPUS];
extern void switch_to_vcpu(pt_regs *regs);

DEFINE_PER_CPU(vcpu_t *, current_vcpu);
DEFINE_PER_CPU(vcpu_t *, next_vcpu);
DEFINE_PER_CPU(pcpu_t *, pcpu);

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

	/*
	 * idle vcpu for each pcpu
	 */
	if (vcpu->vm == NULL) {
		list_add_tail(&pcpu->vcpu_list, &vcpu->pcpu_list);
		return affinity;
	}

	pcpu = &pcpus[affinity];
	list_for_each(&pcpu->vcpu_list, list) {
		tvcpu = list_entry(list, vcpu_t, pcpu_list);
		if ((vcpu->vm->vmid) ==
				(tvcpu->vm->vmid))
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
			if ((vcpu->vm->vmid) ==
					(tvcpu->vm->vmid)) {
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
	pcpu_t *pcpu;
	struct list_head *list;
	vcpu_t *vcpu;

	pcpu = get_cpu_var(pcpu);

	while (1) {
resched:
		vcpu = find_vcpu_to_run(pcpu);

		if ((vcpu == NULL) && (!pcpu->need_resched)) {
			/*
			 * if there is no vcpu to sched and
			 * do not need to resched, then this
			 * pcpu can goto idle state
			 */
			cpu_idle();
		} else {
			if (pcpu->need_resched)
				goto resched;

			get_cpu_var(current_vcpu) = vcpu;
			break;
		}
	}
}

void vmm_pcpus_init(void)
{
	int i;
	pcpu_t *pcpu;

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		pcpu = &pcpus[i];
		init_list(&pcpu->vcpu_list);
		pcpu->pcpu_id = i;
		get_per_cpu(pcpu, i) = pcpu;
	}
}
