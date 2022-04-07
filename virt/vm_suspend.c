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
