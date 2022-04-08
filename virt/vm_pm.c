/*
 * Copyright (C) 2022 Min Le (lemin9538@gmail.com)
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
#include <minos/mm.h>
#include <virt/os.h>
#include <virt/vm.h>
#include <virt/vmodule.h>
#include <virt/virq.h>
#include <virt/vmm.h>
#include <virt/vdev.h>
#include <minos/task.h>
#include <minos/pm.h>
#include <virt/virt.h>
#include <virt/vmcs.h>
#include <virt/vm_pm.h>

static void vcpu_power_off_call(void *data)
{
	struct vcpu *vcpu = (struct vcpu *)data;

	if (current != vcpu->task)
		set_need_resched();
}

void vcpu_enter_poweroff(struct vcpu *vcpu)
{
	struct task *task = vcpu->task;

	if (check_vcpu_state(vcpu, VCPU_STATE_STOP) ||
			check_vcpu_state(vcpu, VCPU_STATE_SUSPEND))
		return;

	task_need_freeze(task);
	if (vcpu->task == current)
		return;

	wake_up_abort(task);

	if ((vcpu_affinity(vcpu) != smp_processor_id())) {
		pr_debug("call vcpu_power_off_call for vcpu-%s\n",
				vcpu->task->name);
		smp_function_call(vcpu->task->affinity,
				vcpu_power_off_call, (void *)vcpu, 0);
	}
}

static int wait_vcpu_in_state(struct vcpu *vcpu, int state, uint32_t timeout)
{
	int i;

	timeout = (timeout == 0) ? -1 : timeout;
	pr_notice("wait %d ms for vcpu%d in %s goto state %d\n",
			timeout, vcpu->vcpu_id, vcpu->vm->name, state);

	/*
	 * if the target vcpu is on the same physical pcpu, need
	 * call sched() to let it has time to run.
	 */
	for (i = 0; i < timeout / 10; i++) {
		if (vcpu->task->state == state)
			return 0;

		msleep(10);
	}

	return -ETIMEDOUT;
}

static inline int wait_vm_in_state(struct vm *vm, int state)
{
	struct vcpu *vcpu;

	vm_for_each_vcpu(vm, vcpu) {
		if (wait_vcpu_in_state(vcpu, TASK_STATE_SUSPEND, 1000)) {
			pr_err("vm-%d vcpu-%d power off failed\n",
					vm->vmid, vcpu->vcpu_id);
			return -EBUSY;
		}
	}

	pr_notice("all vcpu in %s has been suspend\n", vm->name);

	return 0;
}

static int shutdown_all_guest_vm(void)
{
	struct vm *gvm;
	int i;

	for (i = 1; i < CONFIG_MAX_VM; i++) {
		gvm = vms[i];
		if (!gvm || vm_is_native(gvm))
			continue;

		vm_power_off(gvm->vmid, NULL, VM_PM_ACTION_BY_HOST);
		pr_notice("waitting vm%d %s shutdown\n", gvm->vmid, gvm->name);

		/*
		 * do a hardware reset ?
		 */
		if (wait_vm_in_state(gvm, TASK_STATE_SUSPEND))
			return -ETIMEDOUT;

		destroy_vm(gvm);
	}

	return 0;
}

static void vcpu_reset(struct vcpu *vcpu)
{
	/*
	 * 1 - reset the vcpu vmodule state (vgic, context)
	 * 2 - reset the vcpu's irq struct.
	 */
	reset_vcpu_vmodule_state(vcpu);
	vcpu_virq_struct_reset(vcpu);
}

static void __native_vm_reset(struct vm *vm)
{
	struct vdev *vdev;
	struct vcpu *vcpu;

	pr_notice("native vm %s reboot\n", vm->name);

	vm_for_each_vcpu(vm, vcpu)
		vcpu_reset(vcpu);

	/* reset the vdev for this vm */
	list_for_each_entry(vdev, &vm->vdev_list, list) {
		if (vdev->reset)
			vdev->reset(vdev);
	}

	vm_virq_reset(vm);
}

int reboot_native_vm(struct vm *vm)
{
	if (wait_vm_in_state(vm, TASK_STATE_SUSPEND))
		return -EBUSY;

	/*
	 * if reboot the host vm, need to kill all the guest
	 * vm in system if has. here shutdown all the guest
	 * vm.
	 */
	if (vm_is_host_vm(vm))
		shutdown_all_guest_vm();

	__native_vm_reset(vm);
	start_native_vm(vm);

	return 0;
}

static inline int native_vm_reset(struct vm *vm, int flags)
{
	if (pm_action_by_self(flags)) {
		pr_notice("send REBOOT request to VM daemon\n");
		return send_vm_reboot_request(vm);
	} else {
		/*
		 * if called by hypervisor, then it already in another
		 * task context, so can call the reset action directly
		 * and get the status.
		 */
		return reboot_native_vm(vm);
	}
}

static inline int guest_vm_reset(struct vm *vm, int flags)
{
	pr_notice("send REBOOT request to mvm\n");
	return trap_vcpu_nonblock(VMTRAP_TYPE_COMMON,
			VMTRAP_REASON_REBOOT, 0, NULL);
}

static int __vm_reset(struct vm *vm, void *args, int flags)
{
	struct vcpu *vcpu;

	set_vm_state(vm, VM_STATE_REBOOT);
	vm_for_each_vcpu(vm, vcpu)
		vcpu_enter_poweroff(vcpu);

	if (vm_is_native(vm))
		return native_vm_reset(vm, flags);
	else
		return guest_vm_reset(vm, flags);
}

int vm_reset(int vmid, void *args, int flags)
{
	struct vm *vm;

	vm = get_vm_by_id(vmid);
	if (!vm)
		return -ENOENT;

	pr_notice("reset vm-%d by %s\n",
			vm->vmid, pm_action_caller(flags));
	if (vm_is_native(vm) && (!(vm->flags & VM_FLAGS_CAN_RESET) ||
				pm_action_by_mvm(flags))) {
		pr_err("vm%d do not support reset\n", vm->vmid);
		return -EPERM;
	} else if (!vm_is_native(vm) && pm_action_by_host(flags)) {
		pr_err("guest vm can not reset by host\n");
		return -EPERM;
	}

	return __vm_reset(vm, args, flags);
}

int shutdown_native_vm(struct vm *vm)
{
	if (wait_vm_in_state(vm, TASK_STATE_SUSPEND)) {
		pr_warn("vm %s shutdown failed\n", vm->name);
		return -EBUSY;
	}

	if (vm_is_host_vm(vm))
		shutdown_all_guest_vm();

	__native_vm_reset(vm);

	return 0;
}

static int __shutdown_native_vm(struct vm *vm, int flags)
{
	if (!pm_action_by_self(flags))
		return 0;

	pr_notice("send SHUTDOWN request to mvm\n");
	return send_vm_shutdown_request(vm);
}

static int __shutdown_guest_vm(struct vm *vm, int flags)
{
	if (!pm_action_by_self(flags))
		return 0;

	pr_notice("send SHUTDOWN request to mvm\n");
	return trap_vcpu_nonblock(VMTRAP_TYPE_COMMON,
			VMTRAP_REASON_SHUTDOWN, 0, NULL);
}

static int __vm_power_off(struct vm *vm, void *args, int flags)
{
	struct vcpu *vcpu;

	if (!vm_is_native(vm))
		flags |= VM_PM_FLAGS_DESTROY;

	set_vm_state(vm, VM_STATE_OFFLINE);
	vm_for_each_vcpu(vm, vcpu)
		vcpu_enter_poweroff(vcpu);

	/*
	 * the vcpu has been set to TIF_NEED_STOP, so when return
	 * to guest, the task will be killed by kernel.
	 */
	if (vm_is_native(vm))
		return __shutdown_native_vm(vm, flags);
	else
		return __shutdown_guest_vm(vm, flags);
}

int vm_power_off(int vmid, void *arg, int flags)
{
	struct vm *vm = NULL;

	vm = get_vm_by_id(vmid);
	if (!vm)
		return -EINVAL;

	pr_notice("shutdown vm-%d by %s\n",
			vm->vmid, pm_action_caller(flags));
	if (vm_is_native(vm) && (!(vm->flags & VM_FLAGS_CAN_RESET) ||
				pm_action_by_mvm(flags))) {
		pr_err("vm%d do not support shutdown\n", vm->vmid);
		return -EPERM;
	} else if (!vm_is_native(vm) && pm_action_by_host(flags)) {
		pr_err("guest vm can not shutdown by host\n");
		return -EPERM;
	}

	return __vm_power_off(vm, arg, flags);
}

static int vm_resume(struct vm *vm)
{
	struct vcpu *vcpu;

	pr_notice("vm-%d resumed\n", vm->vmid);

	vm_for_each_vcpu(vm, vcpu) {
		if (get_vcpu_id(vcpu) == 0)
			continue;

		resume_vcpu_vmodule_state(vcpu);
	}

	do_hooks((void *)vm, NULL, OS_HOOK_RESUME_VM);

	if (!vm_is_native(vm)) {
		pr_notice("send VM RESUME request to mvm\n");
		trap_vcpu_nonblock(VMTRAP_TYPE_COMMON,
				VMTRAP_REASON_VM_RESUMED, 0, NULL);
	}

	return 0;
}

static int __vm_suspend(struct vm *vm)
{
	struct vcpu *vcpu;

	pr_notice("suspend vm-%d\n", vm->vmid);
	if (get_vcpu_id(current_vcpu) != 0) {
		pr_err("vm suspend can only called by vcpu0\n");
		return -EPERM;
	}

	vm_for_each_vcpu(vm, vcpu) {
		if (vcpu == current_vcpu)
			continue;

		if (!check_vcpu_state(vcpu, TASK_STATE_STOP)) {
			pr_err("vcpu-%d is not suspend vm suspend fail\n",
					get_vcpu_id(vcpu));
			return -EINVAL;
		}

		/*
		 * other VCPU will powered up by vcpu0 again when
		 * it is suspended.
		 */
		suspend_vcpu_vmodule_state(vcpu);
	}

	vm->state = VM_STATE_SUSPEND;
	smp_mb();

	do_hooks((void *)vm, NULL, OS_HOOK_SUSPEND_VM);

	if (!vm_is_native(vm)) {
		pr_notice("send VM SUSPEND request to mvm\n");
		trap_vcpu_nonblock(VMTRAP_TYPE_COMMON,
				VMTRAP_REASON_VM_SUSPEND, 0, NULL);
	}

	/*
	 * vcpu0 will set to WAIT_EVENT state, so the interrupt
	 * can wakeup it, other VCPU will set to STOP state. Only
	 * can be wake up by vcpu0.
	 */
	vcpu_idle(current_vcpu);

	/*
	 * vm is resumed
	 */
	vm->state = VM_STATE_ONLINE;
	smp_wmb();

	vm_resume(vm);

	return 0;
}

int vm_suspend(int vmid)
{
	struct vm *vm = get_vm_by_id(vmid);

	if (vm == NULL)
		return -EINVAL;

	return __vm_suspend(vm);
}

int power_up_guest_vm(int vmid)
{
	struct vm *vm = get_vm_by_id(vmid);

	if (vm == NULL)
		return -ENOENT;

	return start_guest_vm(vm);
}
