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

static struct vm *vms[CONFIG_MAX_VM];
static uint32_t total_vms = 0;

LIST_HEAD(vm_list);

void vcpu_online(struct vcpu *vcpu)
{
	int cpuid = get_cpu_id();

	if (vcpu->affinity != cpuid) {
		vcpu->resched = 1;
		pcpu_resched(vcpu->affinity);
		return;
	}

	set_vcpu_ready(vcpu);
}

void vcpu_offline(struct vcpu *vcpu)
{
	set_vcpu_suspend(vcpu);
}

int vcpu_power_on(struct vcpu *caller, int cpuid,
		unsigned long entry, unsigned long unsed)
{
	struct vcpu *vcpu;
	struct os *os = caller->vm->os;

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
		pr_error("no such:%d vcpu for this VM %s\n",
				cpuid, caller->vm->name);
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

static int add_vm(struct vmtag *vme)
{
	struct vm *vm;

	if (!vme)
		return -EINVAL;

	vm = (struct vm *)malloc(sizeof(struct vm));
	if (!vm)
		return -ENOMEM;

	memset((char *)vm, 0, sizeof(struct vm));
	vm->vmid = vme->vmid;
	strncpy(vm->name, vme->name,
		MIN(strlen(vme->name), MINOS_VM_NAME_SIZE - 1));
	strncpy(vm->os_type, vme->type,
		MIN(strlen(vme->type), OS_TYPE_SIZE - 1));
	vm->vcpu_nr = MIN(vme->nr_vcpu, CONFIG_VM_MAX_VCPU);
	vm->mmu_on = vme->mmu_on;
	vm->entry_point = vme->entry;
	vm->setup_data = vme->setup_data;
	memcpy(vm->vcpu_affinity, vme->vcpu_affinity,
			sizeof(uint32_t) * CONFIG_VM_MAX_VCPU);

	vm->index = total_vms;
	vms[total_vms] = vm;
	total_vms++;
	list_add_tail(&vm_list, &vm->vm_list);

	vmodules_create_vm(vm);

	return 0;
}

struct vm *get_vm_by_id(uint32_t vmid)
{
	int i;
	struct vm *vm;

	for (i = 0; i < total_vms; i++) {
		vm = vms[i];
		if (vm->vmid == vmid)
			return vm;
	}

	return NULL;
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
	free(vcpu->stack_origin);
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

	vcpu_virq_struct_init(vcpu->virq_struct);
	vm->vcpus[vcpu_id] = vcpu;

	return vcpu;
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

		arch_init_vcpu(vcpu, (void *)vm->entry_point);
		pcpu_add_vcpu(vcpu->affinity, vcpu);
	}

	return 0;
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
	set_vcpu_ready(idle);
	idle->state = VCPU_STAT_RUNNING;
	idle->affinity = get_cpu_id();

	strncpy(idle->name, "idle", 4);

	current_vcpu = idle;
	next_vcpu = idle;

	return idle;
}

static void inline vm_vmodules_init(struct vm *vm)
{
	int i;

	for (i = 0; i < vm->vcpu_nr; i++)
		vcpu_vmodules_init(vm->vcpus[i]);
}

int vms_init(void)
{
	int i;
	struct vm *vm;

	for_each_vm(vm) {
		/*
		 * - map the vm's memory
		 * - create the vcpu for vm's each vcpu
		 * - init the vmodule state for each vcpu
		 * - prepare the vcpu for bootup
		 */
		vm_mm_init(vm);
		create_vcpus(vm);
		vm->os = get_vm_os(vm->os_type);
		vm_vmodules_init(vm);

		for (i = 0; i < vm->vcpu_nr; i++)
			vm->os->ops->vcpu_init(vm->vcpus[i]);
	}

	return 0;
}

int create_vms(void)
{
	int i, count = 0;
	struct vmtag *vmtags = mv_config->vmtags;

	if (mv_config->nr_vmtag == 0) {
		pr_error("no VM is found\n");
		return -ENOENT;
	}

	pr_info("found %d VMs config\n", mv_config->nr_vmtag);

	for (i = 0; i < mv_config->nr_vmtag; i++) {
		if (add_vm(&vmtags[i])) {
			pr_error("create %d VM:%s failed\n", i, vmtags[i].name);
			continue;
		}

		count++;
	}

	return count;
}
