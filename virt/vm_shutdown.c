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
#include <virt/vm_pm.h>

static int shutdown_native_vm(struct vm *vm, int flags)
{
	if (!pm_action_by_self(flags))
		return 0;

	pr_notice("send SHUTDOWN request to mvm\n");
	return trap_vcpu_nonblock(VMTRAP_TYPE_COMMON,
			VMTRAP_REASON_SHUTDOWN, 0, NULL);
}

static int shutdown_guest_vm(struct vm *vm, int flags)
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

	/*
	 * natvie VM do not support powered off by other.
	 */
	ASSERT(vm_is_native(vm) && pm_action_by_self(flags));
	pr_notice("power off vm-%d by %s\n", vm->vmid,
			pm_action_caller(flags));

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
		return shutdown_native_vm(vm, flags);
	else
		return shutdown_guest_vm(vm, flags);
}

int vm_power_off(int vmid, void *arg, int flags)
{
	struct vm *vm = NULL;

	vm = get_vm_by_id(vmid);
	if (!vm)
		return -EINVAL;

	return __vm_power_off(vm, arg, flags);
}
