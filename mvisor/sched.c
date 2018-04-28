#include <mvisor/vcpu.h>
#include <mvisor/sched.h>
#include <mvisor/vm.h>
#include <mvisor/mvisor.h>
#include <mvisor/percpu.h>
#include <mvisor/pm.h>
#include <mvisor/module.h>
#include <mvisor/irq.h>
#include <mvisor/list.h>
#include <mvisor/timer.h>
#include <mvisor/time.h>

static struct pcpu pcpus[CONFIG_NR_CPUS];

#define VCPU_MIN_RUN_TIME	(MILLISECS(2))

DEFINE_PER_CPU(struct vcpu *, current_vcpu);
DEFINE_PER_CPU(struct vcpu *, next_vcpu);
DEFINE_PER_CPU(struct pcpu *, pcpu);

uint32_t pcpu_affinity(struct vcpu *vcpu, uint32_t affinity)
{
	int i, found;
	uint32_t af;
	struct pcpu *pcpu;
	struct list_head *list;
	struct vcpu *tvcpu;

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
		tvcpu = list_entry(list, struct vcpu, pcpu_list);
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
			tvcpu = list_entry(list, struct vcpu, pcpu_list);
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

static struct vcpu *find_vcpu_to_run(struct pcpu *pcpu)
{
	unsigned long flags;
	struct vcpu *vcpu = NULL;

	/*
	 * TBD
	 */
	local_irq_save(flags);

	if (is_list_empty(&pcpu->ready_list))
		goto out;

	vcpu = list_first_entry(&pcpu->ready_list,
			struct vcpu, state_list);

out:
	local_irq_restore(flags);
	return vcpu;
}

int vcpu_sched_init(struct vcpu *vcpu)
{
	struct pcpu *pcpu = get_per_cpu(pcpu, vcpu->pcpu_affinity);

	init_list(&vcpu->state_list);
	set_vcpu_state(vcpu, VCPU_STATE_READY);
	list_add_tail(&pcpu->ready_list, &vcpu->state_list);
	vcpu->run_start = 0;
	vcpu->run_time = MILLISECS(CONFIG_SCHED_INTERVAL);

	return 0;
}

static inline int is_vcpu_ready(struct vcpu *vcpu)
{
	return (vcpu->state_list.next != NULL);
}

static inline int detach_ready_vcpu(struct vcpu *vcpu)
{
	if (!is_vcpu_ready(vcpu))
		return 1;

	list_del(&vcpu->state_list);
	vcpu->state_list.next = NULL;
}

static void update_sched_info(struct vcpu *vcpu)
{
	unsigned long now, delta;

	now = NOW();
	delta = now - vcpu->run_start;

	if (delta > vcpu->run_time)
		vcpu->run_time = 0;
	else
		vcpu->run_time = vcpu->run_time - delta;
}

void sched_vcpu(struct vcpu *vcpu, int reason)
{
	struct vcpu *current = current_vcpu();
	struct pcpu *pcpu = get_cpu_var(pcpu);
	unsigned long flags;

	if (vcpu == current)
		return;

	local_irq_save(flags);

	update_sched_info(current);

	/*
	 * add the current vcpu to the tail of the ready
	 * list and put the running vcpu to head
	 */
	list_add_tail(&pcpu->ready_list, &current->state_list);

	detach_ready_vcpu(vcpu);
	list_add(&pcpu->ready_list, &vcpu->state_list);

	local_irq_restore(flags);

	sched();
}

void sched(void)
{
	struct pcpu *pcpu;
	struct list_head *list;
	struct vcpu *vcpu;
	unsigned long flag;

	pcpu = get_cpu_var(pcpu);

	while (1) {
resched:
		pcpu->need_resched = 0;

		vcpu = find_vcpu_to_run(pcpu);

		if ((vcpu == NULL) && (!pcpu->need_resched)) {
			/*
			 * if there is no vcpu to sched and
			 * do not need to resched, then this
			 * pcpu can goto idle state
			 */
			cpu_idle();
		} else {
			get_cpu_var(next_vcpu) = vcpu;

			if (pcpu->need_resched)
				goto resched;

			break;
		}
	}
}

static void sched_timer_function(unsigned long data)
{
	struct pcpu *pcpu = get_cpu_var(pcpu);
	struct timer_list *timer = &pcpu->sched_timer;
	struct vcpu *vcpu = current_vcpu();
	unsigned long flags;
	struct vcpu *next = vcpu;

	/*
	 * if the current running vcpu is NULL
	 * then the pcpu is in idle mode or there is
	 * still run time for the vcpu return directly
	 */
	if (vcpu == NULL)
		return;

	update_sched_info(vcpu);

	if ((vcpu->run_time >= VCPU_MIN_RUN_TIME))
		goto out;

	/*
	 * just delete it from the list and add it
	 * to the tail of the ready list
	 */
	local_irq_save(flags);
	list_add_tail(&pcpu->ready_list, &vcpu->state_list);
	local_irq_restore(flags);

	sched();

	next = get_cpu_var(next_vcpu);
	next->run_time += MILLISECS(CONFIG_SCHED_INTERVAL);

out:
	mod_timer(&pcpu->sched_timer, vcpu->run_time);
}

int vcpu_can_idle(struct vcpu *vcpu)
{
	/*
	 * check whether there irq do not handled
	 */
	if (vcpu_has_virq(vcpu))
		return 0;

	if (vcpu->state != VCPU_STATE_RUNNING)
		return  0;

	return 1;
}

void vcpu_idle(struct vcpu *vcpu)
{
	unsigned long flag;
	int cpuid = get_cpu_id();
	struct pcpu *pcpu = get_per_cpu(pcpu, cpuid);

	if (!vcpu_can_idle(vcpu))
		return;

	pr_debug("vcpu_idle for vcpu:%d\n", get_vcpu_id(vcpu));

	local_irq_save(flag);

	update_sched_info(vcpu);

	set_vcpu_state(vcpu, VCPU_STATE_SLEEP);
	detach_ready_vcpu(vcpu);

	save_vcpu_module_state(vcpu);
	get_per_cpu(current_vcpu, cpuid) = NULL;
	get_per_cpu(next_vcpu, cpuid) = NULL;

	local_irq_restore(flag);

	sched();
}

void switch_to_vcpu(struct vcpu *current, struct vcpu *next)
{
	struct pcpu *pcpu = get_cpu_var(pcpu);
	unsigned long now = NOW();

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

		next->run_start = now;
		set_vcpu_state(next, VCPU_STATE_RUNNING);
		detach_ready_vcpu(next);
		restore_vcpu_module_state(next);
	}

	mvisor_enter_to_guest(next);

	/*
	 * here need to deal the cache and tlb
	 * TBD
	 */
}

static void sched_exit_from_guest(struct vcpu *vcpu, void *data)
{

}

static void sched_enter_to_guest(struct vcpu *vcpu, void *data)
{

}

void mvisor_pcpus_init(void)
{
	int i;
	struct pcpu *pcpu;

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		pcpu = &pcpus[i];
		pcpu->state = PCPU_STATE_RUNNING;
		init_list(&pcpu->vcpu_list);
		init_list(&pcpu->ready_list);
		pcpu->pcpu_id = i;

		/*
		 * init the sched timer
		 */
		init_timer(&pcpu->sched_timer);
		pcpu->sched_timer.function = sched_timer_function;

		get_per_cpu(pcpu, i) = pcpu;
	}

	mvisor_register_hook(sched_exit_from_guest,
			NULL, MVISOR_HOOK_TYPE_EXIT_FROM_GUEST);
	mvisor_register_hook(sched_enter_to_guest,
			NULL, MVISOR_HOOK_TYPE_ENTER_TO_GUEST);
}

static int vcpu_need_to_run(struct vcpu *vcpu)
{
	if (vcpu_has_virq_pending(vcpu))
		return 1;

	return 0;
}

int mvisor_reched_handler(uint32_t irq, void *data)
{
	struct pcpu *pcpu = get_cpu_var(pcpu);
	struct vcpu *vcpu;

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
	struct timer_list *timer;
	struct pcpu *pcpu = get_cpu_var(pcpu);

	ret = request_irq(CONFIG_MVISOR_RESCHED_IRQ,
			mvisor_reched_handler, NULL);

	timer = &pcpu->sched_timer;
	timer->expires = MILLISECS(CONFIG_SCHED_INTERVAL);
	add_timer(timer);

	return 0;
}

device_initcall_percpu(sched_late_init);
