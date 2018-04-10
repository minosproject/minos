#include <mvisor/mvisor.h>
#include <mvisor/vm.h>
#include <mvisor/vcpu.h>
#include <mvisor/sched.h>
#include <config/vm_config.h>
#include <config/config.h>
#include <mvisor/module.h>
#include <mvisor/mm.h>
#include <mvisor/bitmap.h>
#include <mvisor/irq.h>

extern unsigned char __vmm_vm_start;
extern unsigned char __vmm_vm_end;

static struct vm *vms[CONFIG_MAX_VM];
static uint32_t total_vms = 0;

struct list_head vm_list;

static int vmm_add_vm(vm_entry_t *vme)
{
	int i;
	struct vm *vm;

	if (!vme)
		return -EINVAL;

	vm = (struct vm *)vmm_malloc(sizeof(struct vm));
	if (!vm)
		return -ENOMEM;

	memset((char *)vm, 0, sizeof(struct vm));
	vm->vmid = vme->vmid;
	strncpy(vm->name, vme->name,
		MIN(strlen(vme->name), VMM_VM_NAME_SIZE - 1));
	vm->vcpu_nr = MIN(vme->nr_vcpu, CONFIG_VM_MAX_VCPU);
	vm->boot_vm = (boot_vm_t)vme->boot_vm;
	vm->mmu_on = vme->mmu_on;
	vm->entry_point = vme->entry_point;
	memcpy(vm->vcpu_affinity, vme->vcpu_affinity,
			sizeof(uint32_t) * CONFIG_VM_MAX_VCPU);

	vm->index = total_vms;
	vms[total_vms] = vm;
	total_vms++;
	list_add_tail(&vm_list, &vm->vm_list);

	modules_create_vm(vm);

	return 0;
}

static int parse_all_vms(void)
{
	int i;
	vm_entry_t *vme;
	size_t size = (&__vmm_vm_end) - (&__vmm_vm_start);
	unsigned long *start = (unsigned long *)(&__vmm_vm_start);

	if (size == 0) {
		pr_error("No VM is found\n");
		return -ENOENT;
	}

	size = size / sizeof(vm_entry_t *);
	pr_debug("Found %d VMs config\n", size);

	for (i = 0; i < size; i++) {
		vme = (vm_entry_t *)(*start);
		vmm_add_vm(vme);
		start++;
	}

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


static int vm_create_vcpus(struct vm *vm)
{
	int i;
	struct vcpu *vcpu;

	if (!vm)
		return -EINVAL;

	for (i = 0; i < vm->vcpu_nr; i++) {
		vcpu = (struct vcpu *)vmm_malloc(sizeof(struct vcpu));
		if (vcpu == NULL)
			return -ENOMEM;

		memset((char *)vcpu, 0, sizeof(struct vcpu));

		vcpu->vcpu_id = i;
		vcpu->vm = vm;
		vcpu->entry_point = vm->entry_point;
		vcpu->pcpu_affinity = pcpu_affinity(vcpu, vm->vcpu_affinity[i]);
		if (vcpu->pcpu_affinity == PCPU_AFFINITY_FAIL) {
			pr_fatal("%s Can not affinity for vcpu %d\n",
					vm->name, vcpu->vcpu_id);
			panic(NULL);
		} else {
			pr_info("Affinity the vcpu %d to pcpu %d for %s\n",
				vcpu->vcpu_id, vcpu->pcpu_affinity, vm->name);
			vm->vcpu_affinity[i] = vcpu->pcpu_affinity;
		}
		vm->vcpus[i] = vcpu;

		init_list(&vcpu->pcpu_list);
		vcpu_irq_struct_init(&vcpu->irq_struct);
	}

	return 0;
}

static void vm_sched_init(struct vm *vm)
{
	int i;
	struct vcpu *vcpu = NULL;

	for (i = 0; i < vm->vcpu_nr; i++) {
		vcpu = vm->vcpus[i];
		vcpu_sched_init(vcpu);
	}
}

static int inline vm_arch_init(struct vm *vm)
{
	return arch_vm_init(vm);
}

static int vm_modules_init(struct vm *vm)
{
	int i;

	for (i = 0; i < vm->vcpu_nr; i++)
		vcpu_modules_init(vm->vcpus[i]);

	return 0;
}

int vmm_vms_init(void)
{
	int i;
	struct vm *vm;

	for (i = 0; i < total_vms; i++) {
		vm = vms[i];
		vm_memory_init(vm);
		vm_sched_init(vm);
		vm_arch_init(vm);
		vm_modules_init(vm);
	}

	return 0;
}

int vmm_create_vms(void)
{
	int i;
	struct vm *vm;
	int ret = 0;

	init_list(&vm_list);
	ret = parse_all_vms();
	if (ret)
		panic("parsing the vm fail\n");

	for (i = 0; i < total_vms; i++) {
		vm_mm_struct_init(vms[i]);
		vm_create_vcpus(vms[i]);
	}

	return 0;
}
