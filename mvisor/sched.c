#include <mvisor/vcpu.h>
#include <mvisor/sched.h>
#include <mvisor/vm.h>
#include <mvisor/mvisor.h>
#include <mvisor/percpu.h>
#include <mvisor/pm.h>
#include <mvisor/module.h>
#include <mvisor/irq.h>
#include <mvisor/list.h>

static pcpu_t pcpus[CONFIG_NR_CPUS];

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
	if (is_list_empty(&pcpu->ready_list))
		return NULL;

	return list_first_entry(&pcpu->ready_list, vcpu_t, state_list);
}

int vcpu_sched_init(vcpu_t *vcpu)
{
	pcpu_t *pcpu = get_per_cpu(pcpu, vcpu->pcpu_affinity);

	init_list(&vcpu->state_list);
	set_vcpu_state(vcpu, VCPU_STATE_READY);
	list_add_tail(&pcpu->ready_list, &vcpu->state_list);

	return 0;
}

void sched_vcpu(vcpu_t *vcpu, int reason)
{

}

void sched(void)
{
	pcpu_t *pcpu;
	struct list_head *list;
	vcpu_t *vcpu;
	unsigned long flag;

	pcpu = get_cpu_var(pcpu);



	while (1) {
resched:
		local_irq_save(flag);
		vcpu = find_vcpu_to_run(pcpu);

		if ((vcpu == NULL) && (!pcpu->need_resched)) {
			/*
			 * if there is no vcpu to sched and
			 * do not need to resched, then this
			 * pcpu can goto idle state
			 */
			local_irq_restore(flag);
			cpu_idle();
		} else {
			get_cpu_var(next_vcpu) = vcpu;
			local_irq_restore(flag);

			if (pcpu->need_resched)
				goto resched;

			break;
		}
	}
}

int vcpu_can_idle(vcpu_t *vcpu)
{
	/*
	 * check whether there irq do not handled
	 */
	if (vcpu->irq_struct.count)
		return 0;

	if (vcpu->state != VCPU_STATE_RUNNING)
		return  0;

	return 1;
}

void vcpu_idle(vcpu_t *vcpu)
{
	unsigned long flag;
	int cpuid = get_cpu_id();
	pcpu_t *pcpu = get_per_cpu(pcpu, cpuid);

	if (!vcpu_can_idle(vcpu))
		return;

	pr_debug("vcpu_idle for vcpu:%d\n", get_vcpu_id(vcpu));

	local_irq_save(flag);

	set_vcpu_state(vcpu, VCPU_STATE_SLEEP);
	save_vcpu_module_state(vcpu);
	get_per_cpu(current_vcpu, cpuid) = NULL;
	get_per_cpu(next_vcpu, cpuid) = NULL;

	local_irq_restore(flag);

	sched();
}

void switch_to_vcpu(vcpu_t *current, vcpu_t *next)
{
	pcpu_t *pcpu = get_cpu_var(pcpu);

	/*
	 * if current != next and current != NULL
	 * then need to save the current cpu context
	 * to the current vcpu
	 * restore the next vcpu's context to the real
	 * hardware
	 */
	if (current != next) {
		if (current != NULL) {
			set_vcpu_state(current, VCPU_STATE_READY);
			list_add_tail(&pcpu->ready_list, &current->state_list);
			save_vcpu_module_state(current);
		}

		set_vcpu_state(next, VCPU_STATE_RUNNING);
		list_del(&next->state_list);
		restore_vcpu_module_state(next);
	}

	vmm_enter_to_guest(next);

	/*
	 * here need to deal the cache and tlb
	 * TBD
	 */
}

void vmm_pcpus_init(void)
{
	int i;
	pcpu_t *pcpu;

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		pcpu = &pcpus[i];
		init_list(&pcpu->vcpu_list);
		init_list(&pcpu->ready_list);
		pcpu->pcpu_id = i;
		get_per_cpu(pcpu, i) = pcpu;
	}
}

static int vcpu_need_to_run(void)
{
	if (vcpu_has_irq_pending(vcpu))
		return 1;

	return 0;
}

int vmm_reched_handler(uint32_t irq, void *data)
{
	pcpu_t *pcpu = get_cpu_var(pcpu);
	vcpu_t *vcpu;

	/*
	 * ensure the pcpu's member will only modified
	 * by its own cpu thread
	 */
	list_for_each_entry(vcpu, &pcpu->vcpu_list, pcpu_list) {
		if (get_vcpu_state(vcpu) == VCPU_STATE_SLEEP) {
			if (vcpu_need_to_run(vcpu)) {
				set_vcpu_state(vcpu, VCPU_STATE_READY);
				list_add_tail(&pcpu->ready_list, &vcpu->state_list);
			}
		}
	}

	return 0;
}

int sched_late_init(void)
{
	int ret;

	ret = request_irq(CONFIG_VMM_RESCHED_IRQ,
			vmm_reched_handler, NULL);

	pr_debug("request reched irq with error code:%d\n", ret);
	return 0;
}

subsys_initcall_percpu(sched_late_init);
