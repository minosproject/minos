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

static inline int native_vm_reset(struct vm *vm, int flags)
{
	pr_notice("send REBOOT request to VM daemon\n");
	return send_vm_reboot_request(vm);
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

	pr_notice("reset vm-%d by %s\n",
			vm->vmid, pm_action_caller(flags));
	if (vm_is_native(vm) && (!(vm->flags & VM_FLAGS_CAN_RESET) ||
				pm_action_by_mvm(flags))) {
		pr_err("vm%d do not support reset\n", vm->vmid);
		return -EPERM;
	}

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

	return __vm_reset(vm, args, flags);
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

int native_vm_reboot(struct vm *vm)
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

	setup_and_start_vm(vm);

	return 0;
}
