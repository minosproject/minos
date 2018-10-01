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
#include <minos/vcpu.h>
#include <minos/sched.h>
#include <minos/virq.h>

static int vm0_vcpu0_aff = 0;

int __vcpu_trap(uint32_t type, uint32_t reason,
		uint64_t data, uint64_t *ret, int nonblock)
{
	struct vcpu *vcpu = current_vcpu;
	struct vmcs *vmcs = vcpu->vmcs;
	struct vm *vm = get_vm_by_id(0);

	if (vcpu->vmcs_irq < 0) {
		pr_error("no hvm irq for this vcpu\n");
		return -ENOENT;
	}

	if ((type >= VMTRAP_TYPE_UNKNOWN) ||
			(reason >= VMTRAP_REASON_UNKNOWN))
		return -EINVAL;

	/*
	 * wait for the last trap complete, if the gvm
	 * has the same affinity pcpu with the vm0, need
	 * to use sched() in case of dead lock
	 */
	while (vmcs->guest_index != vmcs->host_index) {
		if (vcpu_affinity(vcpu) < vm->vcpu_nr)
			sched();
		else
			cpu_relax();
	}

	vmcs->trap_type = type;
	vmcs->trap_reason = reason;
	vmcs->trap_data = data;
	vmcs->trap_ret = 0;
	if (ret)
		vmcs->trap_result = *ret;
	else
		vmcs->trap_result = 0;

	/*
	 * increase the host index of the vmcs, then send the
	 * virq to the vcpu0 of the vm0
	 */
	vmcs->host_index++;
	dsb();

	send_virq_to_vm(get_vm_by_id(0), vcpu->vmcs_irq);

	/*
	 * if vcpu's pcpu is equal the vm0_vcpu0's pcpu
	 * force to block
	 */
	if (vcpu_affinity(vcpu) == vm0_vcpu0_aff)
		nonblock = 0;

	/*
	 * if gvm's vcpu is on the same pcpu which hvm
	 * affnity, then need to call sched to sched the
	 * hvm's vcpu in case of dead lock
	 */
	if (!nonblock) {
		while (vmcs->guest_index < vmcs->host_index) {
			if (vcpu_affinity(vcpu) < vm->vcpu_nr)
				sched();
			else
				cpu_relax();
		}

		if (ret)
			*ret = vmcs->trap_result;
	} else {
		if (ret)
			*ret = 0;
	}

	return vmcs->trap_ret;
}

int setup_vmcs_data(void *data, size_t size)
{
	void *base = (void *)current_vcpu->vmcs->data;

	if (size > VMCS_DATA_SIZE)
		return -ENOMEM;

	memcpy(base, data, size);
	return 0;
}

static void vcpu_vmcs_init(struct vcpu *vcpu)
{
	struct vmcs *vmcs = vcpu->vmcs;

	if (!vmcs) {
		pr_error("vmcs of vcpu is NULL\n");
		return;
	}

	vmcs->vcpu_id = get_vcpu_id(vcpu);
}

unsigned long vm_create_vmcs(struct vm *vm)
{
	int i;
	uint32_t size;
	struct vcpu *vcpu;
	unsigned long base;

	if (vm->vmcs || vm->hvm_vmcs)
		return 0;

	size = VMCS_SIZE(vm->vcpu_nr);
	base = (unsigned long)get_io_pages(PAGE_NR(size));
	if (!base)
		return 0;

	vm->vmcs = (void *)base;
	memset(vm->vmcs, 0, size);

	vm->hvm_vmcs = (void *)create_hvm_iomem_map(base, size);
	if (!vm->hvm_vmcs) {
		pr_error("mapping vmcs to hvm failed\n");
		free(vm->vmcs);
		return 0;
	}

	for (i = 0; i < vm->vcpu_nr; i++) {
		vcpu = vm->vcpus[i];
		vcpu->vmcs = (struct vmcs *)(vm->vmcs +
				i * sizeof(struct vmcs));
		vcpu_vmcs_init(vcpu);
	}

	return (unsigned long)vm->hvm_vmcs;
}

int vm_create_vmcs_irq(struct vm *vm, int vcpu_id)
{
	struct vcpu *vcpu = get_vcpu_in_vm(vm, vcpu_id);

	if (!vcpu)
		return -ENOENT;

	vcpu->vmcs_irq = alloc_hvm_virq();
	if (vcpu->vmcs_irq < 0)
		pr_error("alloc virq for vmcs failed\n");

	return vcpu->vmcs_irq;
}

static int vmcs_init(void)
{
	struct vcpu *vm0_vcpu0 = NULL;

	vm0_vcpu0 = get_vcpu_by_id(0, 0);
	if (!vm0_vcpu0)
		panic("vm0 is not created\n");

	vm0_vcpu0_aff = vcpu_affinity(vm0_vcpu0);

	return 0;
}

device_initcall(vmcs_init);
