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

void vcpu_dump(struct vcpu *vcpu, gp_regs *regs)
{
	pr_fatal("!!!! VCPU%d of VM%d [%s] fault !!!!\n");
	arch_dump_register(regs);
	dump_vcpu_vmodule_state(vcpu);
}

void vcpu_fault(struct vcpu *vcpu, gp_regs *regs)
{
	struct vm *vm = vcpu->vm;

	vcpu_dump(vcpu, regs);

	/*
	 * shoutdown the VM.
	 */
	vm_power_off(vm->vmid, NULL, VM_PM_ACTION_BY_HOST);
}
