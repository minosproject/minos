/*
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <minos/minos.h>
#include <minos/sched.h>
#include <minos/irq.h>
#include <config/config.h>
#include <minos/mm.h>
#include <minos/bitmap.h>
#include <virt/os.h>
#include <virt/vm.h>
#include <minos/vmodule.h>
#include <virt/virq.h>
#include <virt/vmm.h>
#include <virt/vdev.h>
#include <virt/vmcs.h>
#include <minos/task.h>

extern unsigned char __vm_start;
extern unsigned char __vm_end;

struct vm *vms[CONFIG_MAX_VM];
static int total_vms = 0;

DEFINE_SPIN_LOCK(vms_lock);
static DECLARE_BITMAP(vmid_bitmap, CONFIG_MAX_VM);

static LIST_HEAD(vmtag_list);
LIST_HEAD(vm_list);

static int alloc_new_vmid(void)
{
	int vmid, start = total_vms;

	spin_lock(&vms_lock);
	vmid = find_next_zero_bit_loop(vmid_bitmap, CONFIG_MAX_VM, start);
	if (vmid >= CONFIG_MAX_VM)
		goto out;

	set_bit(vmid, vmid_bitmap);
out:
	spin_unlock(&vms_lock);

	return vmid;
}

static inline void set_vcpu_ready(struct vcpu *vcpu)
{
	unsigned long flags;

	task_lock_irqsave(vcpu->task, flags);
	vcpu->task->stat = TASK_STAT_RDY;
	set_task_ready(vcpu->task, 0);
	task_unlock_irqrestore(vcpu->task, flags);
}

static inline void set_vcpu_stop(struct vcpu *vcpu)
{
	unsigned long flags;

	task_lock_irqsave(vcpu->task, flags);
	vcpu->task->stat = TASK_STAT_STOPPED;
	set_task_sleep(vcpu->task, 0);
	task_unlock_irqrestore(vcpu->task, flags);
}

void vcpu_online(struct vcpu *vcpu)
{
	set_vcpu_ready(vcpu);
}

int vcpu_power_on(struct vcpu *caller, unsigned long affinity,
		unsigned long entry, unsigned long unsed)
{
	int cpuid;
	struct vcpu *vcpu;
	struct os *os = caller->vm->os;

	cpuid = affinity_to_cpuid(affinity);

	/*
	 * resched the pcpu since it may have in the
	 * wfi or wfe state, or need to sched the new
	 * vcpu as soon as possible
	 *
	 * vcpu belong the the same vm will not
	 * at the same pcpu
	 */
	vcpu = get_vcpu_by_id(caller->vm->vmid, cpuid);
	if (!vcpu) {
		pr_err("no such:%d->0x%x vcpu for this VM %s\n",
				cpuid, affinity, caller->vm->name);
		return -ENOENT;
	}

	if (vcpu->task->stat == TASK_STAT_STOPPED) {
		pr_info("vcpu-%d of vm-%d power on from vm suspend 0x%p\n",
				vcpu->vcpu_id, vcpu->vm->vmid, entry);
		os->ops->vcpu_power_on(vcpu, entry);
		vcpu_online(vcpu);
	} else {
		pr_err("vcpu_power_on : invalid vcpu state\n");
		return -EINVAL;
	}

	return 0;
}

int vcpu_can_idle(struct vcpu *vcpu)
{
	if (vcpu_has_irq(vcpu))
		return 0;

	if (vcpu->task->stat != TASK_STAT_RUNNING)
		return 0;

	return 1;
}

void vcpu_idle(struct vcpu *vcpu)
{
	unsigned long flags;

	if (vcpu_can_idle(vcpu)) {
		task_lock_irqsave(vcpu->task, flags);
		if (!vcpu_can_idle(vcpu)) {
			task_unlock_irqrestore(vcpu->task, flags);
			return;
		}

		vcpu->task->stat = TASK_STAT_SUSPEND;
		set_task_sleep(vcpu->task, 0);
		task_unlock_irqrestore(vcpu->task, flags);

		sched();
	}
}

int vcpu_suspend(struct vcpu *vcpu, gp_regs *c,
		uint32_t state, unsigned long entry)
{
	/*
	 * just call vcpu idle to put vcpu to suspend state
	 * and ignore the wake up entry, since the vcpu will
	 * not really powered off
	 */
	vcpu_idle(vcpu);

	return 0;
}

int vcpu_off(struct vcpu *vcpu)
{
	/*
	 * force set the vcpu to suspend state then sched
	 * out
	 */
	set_vcpu_stop(vcpu);
	sched();

	return 0;
}

static int vm_check_vcpu_affinity(int vmid, uint32_t *aff, int nr)
{
	int i;
	uint64_t mask = 0;

	for (i = 0; i < nr; i++) {
		if (aff[i] >= VM_MAX_VCPU)
			return -EINVAL;

		if (mask & (1 << aff[i]))
			return -EINVAL;
		else
			mask |= (1 << aff[i]);
	}

	return 0;
}

static struct vm *__create_vm(struct vmtag *vme)
{
	struct vm *vm;

	if (vm_check_vcpu_affinity(vme->vmid, vme->vcpu_affinity,
				vme->nr_vcpu)) {
		pr_err("vcpu affinit for vm not correct\n");
		return NULL;
	}

	vm = malloc(sizeof(*vm));
	if (!vm)
		return NULL;

	vme->nr_vcpu = MIN(vme->nr_vcpu, VM_MAX_VCPU);

	memset(vm, 0, sizeof(struct vm));
	vm->vcpus = malloc(sizeof(struct vcpu *) * vme->nr_vcpu);
	if (!vm->vcpus) {
		free(vm);
		return NULL;
	}

	vm->vmid = vme->vmid;
	strncpy(vm->name, vme->name, sizeof(vm->name) - 1);
	vm->vcpu_nr = vme->nr_vcpu;
	vm->entry_point = vme->entry;
	vm->setup_data = vme->setup_data;
	vm->state = VM_STAT_OFFLINE;
	init_list(&vm->vdev_list);
	memcpy(vm->vcpu_affinity, vme->vcpu_affinity,
			sizeof(uint32_t) * VM_MAX_VCPU);
	vm->flags |= vme->flags;

	vms[vme->vmid] = vm;
	total_vms++;

	spin_lock(&vms_lock);
	list_add_tail(&vm_list, &vm->vm_list);
	spin_unlock(&vms_lock);

	vm->os = get_vm_os((char *)vme->os_type);

	return vm;
}

struct vcpu *get_vcpu_in_vm(struct vm *vm, uint32_t vcpu_id)
{
	if (vcpu_id >= vm->vcpu_nr)
		return NULL;

	return vm->vcpus[vcpu_id];
}

struct vcpu *get_vcpu_by_id(uint32_t vmid, uint32_t vcpu_id)
{
	struct vm *vm;

	vm = get_vm_by_id(vmid);
	if (!vm)
		return NULL;

	return get_vcpu_in_vm(vm, vcpu_id);
}

void kick_vcpu(struct vcpu *vcpu, int preempt)
{
	unsigned long flags;

	task_lock_irqsave(vcpu->task, flags);
	if (!task_is_ready(vcpu->task)) {
		vcpu->task->stat = TASK_STAT_RDY;
		set_task_ready(vcpu->task, preempt);
	}
	task_unlock_irqrestore(vcpu->task, flags);
}

static void release_vcpu(struct vcpu *vcpu)
{
	if (vcpu->task)
		release_task(vcpu->task);

	if (vcpu->vmcs_irq >= 0)
		release_hvm_virq(vcpu->vmcs_irq);

	free(vcpu->virq_struct);
	free(vcpu);
}

static struct vcpu *alloc_vcpu(void)
{
	struct vcpu *vcpu;

	vcpu = zalloc(sizeof(*vcpu));
	if (!vcpu)
		return NULL;

	vcpu->virq_struct = zalloc(sizeof(struct virq_struct));
	if (!vcpu->virq_struct)
		goto free_vcpu;

	vcpu->vmcs_irq = -1;
	return vcpu;

free_vcpu:
	free(vcpu);

	return NULL;
}

static struct vcpu *create_vcpu(struct vm *vm, uint32_t vcpu_id)
{
	int pid;
	char name[64];
	struct vcpu *vcpu;
	struct task *task;

	/* generate the name of the vcpu task */
	memset(name, 0, 64);
	sprintf(name, "%s-vcpu-%d", vm->name, vcpu_id);
	pid = create_vcpu_task(name, vm->entry_point, NULL,
			vm->vcpu_affinity[vcpu_id], 0);
	if (pid < 0)
		return NULL;

	task = pid_to_task(pid);

	vcpu = alloc_vcpu();
	if (!vcpu) {
		release_task(task);
		return NULL;
	}

	task->pdata = vcpu;
	vcpu->task = task;
	vcpu->vcpu_id = vcpu_id;
	vcpu->vm = vm;

	if (!(vm->flags & VM_FLAGS_64BIT))
		task->flags |= TASK_FLAGS_32BIT;

	init_list(&vcpu->list);

	vcpu_virq_struct_init(vcpu);
	vm->vcpus[vcpu_id] = vcpu;
	spin_lock_init(&vcpu->idle_lock);

	vcpu->next = NULL;
	if (vcpu_id != 0)
		vm->vcpus[vcpu_id - 1]->next = vcpu;

	return vcpu;
}

int vm_vcpus_init(struct vm *vm)
{
	struct vcpu *vcpu;

	vm_for_each_vcpu(vm, vcpu) {
		pr_info("vm-%d vcpu-%d affnity to pcpu-%d\n",
				vm->vmid, vcpu->vcpu_id, vcpu_affinity(vcpu));

		task_vmodules_init(vcpu->task);

		if (!vm_is_native(vm)) {
			vcpu->vmcs->host_index = 0;
			vcpu->vmcs->guest_index = 0;
		}
	}

	/* some task will excuted after this function */
	vm_for_each_vcpu(vm, vcpu)
		vm->os->ops->vcpu_init(vcpu);

	return 0;
}

static int create_vcpus(struct vm *vm)
{
	int i, j;
	struct vcpu *vcpu;

	for (i = 0; i < vm->vcpu_nr; i++) {
		vcpu = create_vcpu(vm, i);
		if (!vcpu) {
			pr_err("create vcpu:%d for %s failed\n", i, vm->name);
			for (j = 0; j < vm->vcpu_nr; j++) {
				vcpu = vm->vcpus[j];
				if (!vcpu)
					continue;

				release_vcpu(vcpu);
			}

			return -ENOMEM;
		}
	}

	return 0;
}

int vcpu_reset(struct vcpu *vcpu)
{
	if (!vcpu)
		return -EINVAL;

	task_vmodules_reset(vcpu->task);
	vcpu_virq_struct_reset(vcpu);

	return 0;
}

void destroy_vm(struct vm *vm)
{
	int i;
	struct vdev *vdev, *n;
	struct vcpu *vcpu;

	if (!vm)
		return;

	/*
	 * 1 : release the vdev
	 * 2 : do hooks for each modules
	 * 3 : release the vcpu allocated to this vm
	 * 4 : free the memory for this VM
	 * 5 : update the vmid bitmap
	 * 6 : do vmodule deinit
	 */
	list_for_each_entry_safe(vdev, n, &vm->vdev_list, list) {
		list_del(&vdev->list);
		if (vdev->deinit)
			vdev->deinit(vdev);
	}

	do_hooks((void *)vm, NULL, OS_HOOK_DESTROY_VM);

	if (vm->vcpus) {
		for (i = 0; i < vm->vcpu_nr; i++) {
			vcpu = vm->vcpus[i];
			if (!vcpu)
				continue;
			release_vcpu(vcpu);
		}

		free(vm->vcpus);
	}

	if (vm->hvm_vmcs)
		destroy_hvm_iomem_map((unsigned long)vm->hvm_vmcs,
				VMCS_SIZE(vm->vcpu_nr));

	if (vm->vmcs)
		free(vm->vmcs);

	vm->hvm_vmcs = NULL;
	vm->vmcs = NULL;
	release_vm_memory(vm);

	i = vm->vmid;
	spin_lock(&vms_lock);
	clear_bit(i, vmid_bitmap);
	list_del(&vm->vm_list);
	spin_unlock(&vms_lock);

	free(vm);
	vms[i] = NULL;
	total_vms--;
}

void vcpu_power_off_call(void *data)
{
	struct vcpu *vcpu = (struct vcpu *)data;

	if (!vcpu)
		return;

	if (vcpu_affinity(vcpu) != smp_processor_id()) {
		pr_err("vcpu-%s do not belong to this pcpu\n",
				vcpu->task->name);
		return;
	}

	set_vcpu_stop(vcpu);
	pr_info("power off vcpu-%d-%d done\n", get_vmid(vcpu),
			get_vcpu_id(vcpu));

	/*
	 * *********** Note ****************
	 * if the vcpu is the current running vcpu
	 * need to resched another vcpu, since the vcpu
	 * may in el2 and el2/el0, force to sched to the
	 * new vcpu
	 */
	if (vcpu == get_current_vcpu()) {
		if (!preempt_allowed())
			pr_err("%s preempt is not allowed\n", __func__);

		set_need_resched();
	}
}

int vcpu_power_off(struct vcpu *vcpu, int timeout)
{
	/*
	 * since the vcpu will not sched on other
	 * cpus which its affinity, so the this
	 * function can called directly
	 */
	int cpuid = smp_processor_id();

	if (vcpu_affinity(vcpu) != cpuid) {
		pr_debug("call vcpu_power_off_call for vcpu-%s\n",
				vcpu->task->name);
		return smp_function_call(vcpu->task->affinity,
				vcpu_power_off_call, (void *)vcpu, 1);
	} else {
		/* just set it stat then force sched to another task */
		set_vcpu_stop(vcpu);
		pr_info("power off vcpu-%d-%d done\n", get_vmid(vcpu),
				get_vcpu_id(vcpu));
	}

	return 0;
}

struct vm *create_vm(struct vmtag *vme)
{
	int ret = 0;
	struct vm *vm;
	int native = 0;

	if (!vme)
		return NULL;

	if ((vme->vmid < 0) || (vme->vmid >= CONFIG_MAX_VM)) {
		vme->vmid = alloc_new_vmid();
		if (vme->vmid == VMID_INVALID)
			return NULL;
	} else {
		if (test_and_set_bit(vme->vmid, vmid_bitmap))
			return NULL;

		native = 1;
	}

	vm = __create_vm(vme);
	if (!vm)
		return NULL;

	if (native)
		vm->flags |= VM_FLAGS_NATIVE;

	vm_mm_struct_init(vm);

	ret = create_vcpus(vm);
	if (ret) {
		pr_err("create vcpus for vm failded\n");
		ret = VMID_INVALID;
		goto release_vm;
	}

	if (do_hooks((void *)vm, NULL, OS_HOOK_CREATE_VM)) {
		pr_err("create vm failed in hook function\n");
		goto release_vm;
	}

	return vm;

release_vm:
	destroy_vm(vm);

	return NULL;
}
