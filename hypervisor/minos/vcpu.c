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
#include <minos/os.h>
#include <minos/virt.h>
#include <minos/vm.h>
#include <minos/vcpu.h>
#include <minos/vmodule.h>
#include <minos/virq.h>
#include <minos/vmm.h>

extern unsigned char __vm_start;
extern unsigned char __vm_end;

struct vm *vms[CONFIG_MAX_VM];
static int total_vms = 0;

DEFINE_SPIN_LOCK(vms_lock);
static DECLARE_BITMAP(vmid_bitmap, CONFIG_MAX_VM);

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

void vcpu_online(struct vcpu *vcpu)
{
	int cpuid = smp_processor_id();

	if (vcpu->affinity != cpuid) {
		vcpu->resched = 1;
		pcpu_resched(vcpu->affinity);
		return;
	}

	set_vcpu_ready(vcpu);
}

int vcpu_power_on(struct vcpu *caller, unsigned long affinity,
		unsigned long entry, unsigned long unsed)
{
	int cpuid;
	struct vcpu *vcpu;
	struct os *os = caller->vm->os;

	cpuid = affinity_to_vcpuid(affinity);

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
		pr_error("no such:%d->0x%x vcpu for this VM %s\n",
				cpuid, affinity, caller->vm->name);
		return -ENOENT;
	}

	if (vcpu->state != VCPU_STAT_IDLE)
		return -EINVAL;

	os->ops->vcpu_power_on(vcpu, entry);

	return 0;
}

int vcpu_can_idle(struct vcpu *vcpu)
{
	if (in_interrupt)
		return 0;

	if (vcpu_has_irq(vcpu))
		return 0;

	if (vcpu->state != VCPU_STAT_RUNNING)
		return 0;

	return 1;
}

void vcpu_idle(void)
{
	struct vcpu *vcpu = current_vcpu;
	unsigned long flags;

	if (vcpu_can_idle(vcpu)) {
		local_irq_save(flags);
		if (vcpu_can_idle(vcpu))
			goto out;

		set_vcpu_suspend(vcpu);
		sched();
out:
		local_irq_restore(flags);
	}
}

int vcpu_suspend(gp_regs *c, uint32_t state, unsigned long entry)
{
	/*
	 * just call vcpu idle to put vcpu to suspend state
	 * and ignore the wake up entry, since the vcpu will
	 * not really powered off
	 */
	vcpu_idle();

	return 0;
}

static struct vm *__create_vm(struct vmtag *vme)
{
	struct vm *vm;

	vm = (struct vm *)malloc(sizeof(struct vm));
	if (!vm)
		return NULL;

	vme->nr_vcpu = MIN(vme->nr_vcpu, VM_MAX_VCPU);

	memset((char *)vm, 0, sizeof(struct vm));
	vm->vcpus = (struct vcpu **)malloc(sizeof(struct vcpu *)
			* vme->nr_vcpu);
	if (!vm->vcpus) {
		free(vm);
		return NULL;
	}

	vm->vmid = vme->vmid;
	strncpy(vm->name, vme->name,
		MIN(strlen(vme->name), MINOS_VM_NAME_SIZE - 1));
	strncpy(vm->os_type, vme->type,
		MIN(strlen(vme->type), OS_TYPE_SIZE - 1));
	vm->vcpu_nr = vme->nr_vcpu;
	vm->entry_point = vme->entry;
	vm->setup_data = vme->setup_data;
	vm->state = VM_STAT_OFFLINE;
	init_list(&vm->vdev_list);
	memcpy(vm->vcpu_affinity, vme->vcpu_affinity,
			sizeof(uint8_t) * VM_MAX_VCPU);
	if (vme->bit64)
		vm->flags |= VM_FLAGS_64BIT;

	vms[vme->vmid] = vm;
	total_vms++;

	spin_lock(&vms_lock);
	list_add_tail(&vm_list, &vm->vm_list);
	spin_unlock(&vms_lock);

	vm_vmodules_init(vm);
	vm->os = get_vm_os(vm->os_type);

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

static void release_vcpu(struct vcpu *vcpu)
{
	if (vcpu->vmodule_context)
		vcpu_vmodules_deinit(vcpu);

	if (vcpu->vmcs_irq >= 0)
		release_hvm_virq(vcpu->vmcs_irq);

	free(vcpu->stack_origin - vcpu->stack_size);
	free(vcpu->virq_struct);
	free(vcpu);
}

static struct vcpu *alloc_vcpu(size_t size)
{
	struct vcpu *vcpu;
	void *stack_base = NULL;

	vcpu = (struct vcpu *)malloc(sizeof(struct vcpu));
	if (!vcpu)
		return NULL;

	memset((char *)vcpu, 0, sizeof(struct vcpu));
	vcpu->virq_struct = malloc(sizeof(struct virq_struct));
	if (!vcpu->virq_struct)
		goto free_vcpu;

	if (size) {
		stack_base = (void *)get_free_pages(PAGE_NR(size));
		if (!stack_base)
			goto free_virq_struct;
	}

	vcpu->stack_size = size;
	vcpu->stack_base = stack_base + size;
	vcpu->stack_origin = vcpu->stack_base;
	vcpu->vmcs_irq = -1;

	return vcpu;

free_virq_struct:
	free(vcpu->virq_struct);
free_vcpu:
	free(vcpu);

	return NULL;
}

static struct vcpu *create_vcpu(struct vm *vm, uint32_t vcpu_id)
{
	char name[64];
	struct vcpu *vcpu;

	vcpu = alloc_vcpu(VCPU_DEFAULT_STACK_SIZE);
	if (!vcpu)
		return NULL;

	vcpu->vcpu_id = vcpu_id;
	vcpu->vm = vm;

	vcpu->affinity = vm->vcpu_affinity[vcpu_id];
	vcpu->state = VCPU_STAT_IDLE;
	vcpu->is_idle = 0;

	init_list(&vcpu->list);
	memset(name, 0, 64);
	sprintf(name, "%s-vcpu-%d", vm->name, vcpu_id);
	strncpy(vcpu->name, name, strlen(name) > (VCPU_NAME_SIZE -1) ?
			(VCPU_NAME_SIZE - 1) : strlen(name));

	vcpu_virq_struct_init(vcpu);
	vm->vcpus[vcpu_id] = vcpu;

	vcpu->next = NULL;
	if (vcpu_id != 0)
		vm->vcpus[vcpu_id - 1]->next = vcpu;

	return vcpu;
}

int vm_vcpus_init(struct vm *vm)
{
	struct vcpu *vcpu;

	if (!vm)
		return -EINVAL;

	vm_for_each_vcpu(vm, vcpu) {
		arch_init_vcpu(vcpu, (void *)vm->entry_point);
		pr_info("vm-%d vcpu-%d affnity to pcpu-%d\n",
				vm->vmid, vcpu->vcpu_id, vcpu->affinity);

		/* only when the vm is offline state do this */
		if (vm->state == VM_STAT_OFFLINE) {
			vcpu_vmodules_init(vcpu);
			pcpu_add_vcpu(vcpu->affinity, vcpu);
		}

		vm->os->ops->vcpu_init(vcpu);

		if (!vm_is_native(vm)) {
			vcpu->vmcs->host_index = 0;
			vcpu->vmcs->guest_index = 0;
		}
	}

	return 0;
}

static int create_vcpus(struct vm *vm)
{
	int i, j;
	struct vcpu *vcpu;

	for (i = 0; i < vm->vcpu_nr; i++) {
		vcpu = create_vcpu(vm, i);
		if (!vcpu) {
			pr_error("create vcpu:%d for %s failed\n", i, vm->name);
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

	vcpu_virq_struct_reset(vcpu);
	sched_reset_vcpu(vcpu);

	return 0;
}

void destroy_vm(struct vm *vm)
{
	int i;
	struct vcpu *vcpu;

	if (!vm)
		return;

	/*
	 * 1 : do hooks for each modules
	 * 2 : release the vcpu allocated to this vm
	 * 3 : free the memory for this VM
	 * 4 : update the vmid bitmap
	 * 5 : do vmodule deinit
	 */
	do_hooks((void *)vm, NULL, MINOS_HOOK_TYPE_DESTROY_VM);

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

	vm_vmodules_deinit(vm);
	free(vm);
	vms[i] = NULL;
	total_vms--;
}

struct vcpu *create_idle_vcpu(void)
{
	struct vcpu *idle = NULL;
	int cpu = smp_processor_id();
	extern unsigned char __el2_stack_end;
	void *el2_stack_base = (void *)&__el2_stack_end;

	idle = alloc_vcpu(0);
	if (!idle)
		panic("Can not create idle vcpu\n");

	init_list(&idle->list);
	idle->stack_size = VCPU_DEFAULT_STACK_SIZE;
	idle->stack_origin = el2_stack_base - (cpu * 0x2000);
	idle->is_idle = 1;

	pcpu_add_vcpu(cpu, idle);
	idle->state = VCPU_STAT_RUNNING;
	idle->affinity = smp_processor_id();

	strncpy(idle->name, "idle", 4);

	current_vcpu = idle;
	next_vcpu = idle;

	return idle;
}

void vcpu_power_off_call(void *data)
{
	struct vcpu *vcpu = (struct vcpu *)data;

	if (!vcpu)
		return;

	if (vcpu->affinity != smp_processor_id()) {
		pr_error("vcpu-%s do not belong to this pcpu\n",
				vcpu->name);
		return;
	}

	set_vcpu_state(vcpu, VCPU_STAT_IDLE);

	/*
	 * if the vcpu is the current running vcpu
	 * need to resched another vcpu
	 */
	if (vcpu == current_vcpu)
		need_resched = 1;

	pr_info("power off vcpu-%d-%d done\n", get_vmid(vcpu),
			get_vcpu_id(vcpu));
}

int vcpu_power_off(struct vcpu *vcpu, int timeout)
{
	int cpuid = smp_processor_id();

	if (!vcpu)
		return -EINVAL;

	if (vcpu->affinity != cpuid) {
		pr_debug("call vcpu_power_off_call for vcpu-%s\n",
				vcpu->name);
		return smp_function_call(vcpu->affinity,
				vcpu_power_off_call, (void *)vcpu, 1);
	} else {
		set_vcpu_state(vcpu, VCPU_STAT_IDLE);
		pr_info("power off vcpu-%d-%d done\n", get_vmid(vcpu),
				get_vcpu_id(vcpu));
	}

	return 0;
}

struct vm *create_vm(struct vmtag *vme)
{
	int ret = 0;
	struct vm *vm;

	if (!vme)
		return NULL;

	if ((vme->vmid < 0) || (vme->vmid >= CONFIG_MAX_VM)) {
		vme->vmid = alloc_new_vmid();
		if (vme->vmid == VMID_INVALID)
			return NULL;
	} else {
		spin_lock(&vms_lock);
		if (test_bit(vme->vmid, vmid_bitmap)) {
			spin_unlock(&vms_lock);
			return NULL;
		}

		set_bit(vme->vmid, vmid_bitmap);
		spin_unlock(&vms_lock);
	}

	vm = __create_vm(vme);
	if (!vm)
		return NULL;

	ret = create_vcpus(vm);
	if (ret) {
		pr_error("create vcpus for vm failded\n");
		ret = VMID_INVALID;
		goto release_vm;
	}

	if (do_hooks((void *)vm, NULL, MINOS_HOOK_TYPE_CREATE_VM)) {
		pr_error("create vm failed in hook function\n");
		goto release_vm;
	}

	do_hooks(vm, NULL, MINOS_HOOK_TYPE_CREATE_VM_VDEV);

	return vm;

release_vm:
	destroy_vm(vm);

	return NULL;
}

int create_static_vms(void)
{
	struct vm *vm;
	int i, count = 0;
	struct vmtag *vmtags = mv_config->vmtags;

	if (mv_config->nr_vmtag == 0) {
		pr_error("no VM is found\n");
		return -ENOENT;
	}

	pr_info("found %d VMs config\n", mv_config->nr_vmtag);

	for (i = 0; i < mv_config->nr_vmtag; i++) {
		vm = create_vm(&vmtags[i]);
		if (!vm) {
			pr_error("create %d VM:%s failed\n", vmtags[i].name);
			continue;
		}

		vm->flags |= VM_FLAGS_NATIVE;
		count++;
	}

	return count;
}

int static_vms_init(void)
{
	struct vm *vm;

	for_each_vm(vm) {
		/*
		 * - map the vm's memory
		 * - create the vcpu for vm's each vcpu
		 * - init the vmodule state for each vcpu
		 * - prepare the vcpu for bootup
		 */
		vm_mm_init(vm);

		if (vm->vmid == 0)
			arch_hvm_init(vm);

		vm_vcpus_init(vm);
		vm->state = VM_STAT_ONLINE;
	}

	return 0;
}
