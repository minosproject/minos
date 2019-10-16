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

static int vm_hvc_handler(gp_regs *c, uint32_t id, uint64_t *args)
{
	int vmid = -1, ret;
	unsigned long addr;
	unsigned long gbase = 0, hbase = 0;
	struct vm *vm = get_vm_by_id((uint32_t)args[0]);

	if (!vm_is_hvm(get_current_vm()))
		panic("only vm0 can call vm related hypercall\n");

	switch (id) {
	case HVC_VM_CREATE:
		vmid = create_guest_vm((struct vmtag *)args[0]);
		HVC_RET1(c, vmid);
		break;

	case HVC_VM_DESTORY:
		destroy_vm(vm);
		HVC_RET1(c, 0);
		break;

	case HVC_VM_RESTART:
		vmid = vm_reset((int)args[0], (void *)c);
		HVC_RET1(c, vmid);
		break;

	case HVC_VM_POWER_UP:
		vmid = vm_power_up((int)args[0]);
		HVC_RET1(c, vmid);
		break;

	case HVC_VM_POWER_DOWN:
		vmid = vm_power_off((int)args[0], (void *)c);
		HVC_RET1(c, vmid);
		break;

	case HVC_VM_MMAP:
		ret = create_vm_mmap((int)args[0], args[1], args[2], &addr);
		HVC_RET2(c, ret, addr);
		break;

	case HVC_VM_UNMMAP:
		destroy_vm_mmap((int)args[0]);
		HVC_RET1(c, 0);
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
		ret = virtio_mmio_init(vm, args[1], &gbase, &hbase);
		HVC_RET3(c, ret, gbase, hbase);
		break;
	case HVC_VM_VIRTIO_MMIO_DEINIT:
		ret = virtio_mmio_deinit(vm);
		HVC_RET1(c, 0);
		break;
	case HVC_VM_CREATE_HOST_VDEV:
		ret = vm_create_host_vdev(vm);
		HVC_RET1(c, ret);
		break;
	case HVC_CHANGE_LOG_LEVEL:
		change_log_level((unsigned int)args[0]);
		break;
	default:
		pr_err("unsupport vm hypercall");
		break;
	}

	HVC_RET1(c, -EINVAL);
}

static int misc_hvc_handler(gp_regs *c, uint32_t id, uint64_t *args)
{
	HVC_RET1(c, -EINVAL);
}

DEFINE_HVC_HANDLER("vm_hvc_handler", HVC_TYPE_HVC_VM0,
		HVC_TYPE_HVC_VM0, vm_hvc_handler);

DEFINE_HVC_HANDLER("misc_hvc_handler", HVC_TYPE_HVC_MISC,
		HVC_TYPE_HVC_MISC, misc_hvc_handler);
