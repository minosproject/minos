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
#include <asm/svccc.h>
#include <minos/sched.h>
#include <virt/vm.h>
#include <virt/hypercall.h>
#include <virt/virq.h>
#include <virt/virtio.h>
#include <virt/vmcs.h>
#include <virt/os.h>
#include <virt/vm_pm.h>

static int vm_hvc_handler(gp_regs *c, uint32_t id, uint64_t *args)
{
	int vmid = -1, ret;
	unsigned long addr;
	unsigned long hbase = 0;
	struct vm *vm = get_vm_by_id((uint32_t)args[0]);

	if (!vm_is_host_vm(get_current_vm()))
		panic("only vm0 can call vm related hypercall\n");

	switch (id) {
	case HVC_VM_CREATE:
		vmid = create_guest_vm((struct vmtag *)args[0]);
		HVC_RET1(c, vmid);
		break;

	case HVC_VM_DESTORY:
		destroy_guest_vm(vm);
		HVC_RET1(c, 0);
		break;

	case HVC_VM_RESTART:
		vmid = vm_reset((int)args[0], NULL, VM_PM_ACTION_BY_MVM);
		HVC_RET1(c, vmid);
		break;

	case HVC_VM_POWER_UP:
		vmid = power_up_guest_vm((int)args[0]);
		HVC_RET1(c, vmid);
		break;

	case HVC_VM_POWER_DOWN:
		vmid = vm_power_off((int)args[0], NULL, VM_PM_ACTION_BY_MVM);
		HVC_RET1(c, vmid);
		break;

	case HVC_VM_MMAP:
		ret = create_vm_mmap((int)args[0], args[1], args[2], &addr);
		HVC_RET2(c, ret, addr);
		break;

	case HVC_VM_SEND_VIRQ:
		send_virq_to_vm(get_vm_by_id((int)args[0]), (int)args[1]);
		HVC_RET1(c, 0);
		break;

	case HVC_VM_CREATE_VMCS:
		addr = vm_create_vmcs(vm);
		HVC_RET1(c, addr);
		break;

	case HVC_VM_REQUEST_VIRQ:
		vmid = request_vm_virqs(get_vm_by_id((int)args[0]),
				(int)args[1], (int)args[2]);
		HVC_RET1(c, vmid);
		break;

	case HVC_VM_CREATE_VMCS_IRQ:
		vmid = vm_create_vmcs_irq(vm, (int)args[1]);
		HVC_RET1(c, vmid);
		break;

	case HVC_VM_VIRTIO_MMIO_INIT:
		ret = virtio_mmio_init(vm, args[1], args[2], &hbase);
		HVC_RET2(c, ret, hbase);
		break;
	case HVC_VM_CREATE_RESOURCE:
		ret = os_create_guest_vm_resource(vm);
		HVC_RET1(c, ret);
		break;
	case HVC_CHANGE_LOG_LEVEL:
		change_log_level((unsigned int)args[0]);
		break;
	case HVC_DUMP_LOG:
		dump_system_log();
		break;
	default:
		pr_err("unsupport vm hypercall");
		break;
	}

	HVC_RET1(c, -EINVAL);
}

#define CHECK_VM_CAP(vm, cap, flg, value) \
	(value) |= (flg) & VM_FLAGS_HOST ? (cap) : 0;

static unsigned long get_vm_capability(struct vm *vm)
{
	unsigned long ret = 0;

	CHECK_VM_CAP(vm, VM_CAP_HOST, VM_FLAGS_HOST, ret);
	CHECK_VM_CAP(vm, VM_CAP_NATIVE, VM_FLAGS_NATIVE, ret);

	return ret;
}

static int misc_hvc_handler(gp_regs *c, uint32_t id, uint64_t *args)
{
	struct vm *vm = get_current_vm();

	switch (id) {
	case HVC_GET_VMID:
		HVC_RET1(c, vm->vmid);
		break;
	case HVC_SCHED_OUT:
		sched();
		HVC_RET1(c, 0);
		break;
	case HVC_GET_VM_CAP:
		HVC_RET1(c, get_vm_capability(vm));
		break;
	default:
		break;
	}

	HVC_RET1(c, -EINVAL);
}

DEFINE_HVC_HANDLER("vm_hvc_handler", HVC_TYPE_VM0,
		HVC_TYPE_VM0, vm_hvc_handler);

DEFINE_HVC_HANDLER("misc_hvc_handler", HVC_TYPE_MISC,
		HVC_TYPE_MISC, misc_hvc_handler);
