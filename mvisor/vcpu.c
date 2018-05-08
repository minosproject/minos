#include <mvisor/mvisor.h>
#include <mvisor/vm.h>
#include <mvisor/vcpu.h>
#include <mvisor/sched.h>
#include <config/config.h>
#include <mvisor/module.h>
#include <mvisor/mm.h>
#include <mvisor/bitmap.h>
#include <mvisor/irq.h>
#include <mvisor/mvisor_config.h>
#include <mvisor/os.h>

extern unsigned char __mvisor_vm_start;
extern unsigned char __mvisor_vm_end;

static struct vm *vms[CONFIG_MAX_VM];
static uint32_t total_vms = 0;

LIST_HEAD(vm_list);

static int mvisor_add_vm(struct mvisor_vmtag *vme)
{
	struct vm *vm;

	if (!vme)
		return -EINVAL;

	vm = (struct vm *)mvisor_malloc(sizeof(struct vm));
	if (!vm)
		return -ENOMEM;

	memset((char *)vm, 0, sizeof(struct vm));
	vm->vmid = vme->vmid;
	strncpy(vm->name, vme->name,
		MIN(strlen(vme->name), MVISOR_VM_NAME_SIZE - 1));
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

	modules_create_vm(vm);

	return 0;
}

static int parse_all_vms(void)
{
	int i;
	struct mvisor_vmtag *vmtags = mv_config->vmtags;

	if (mv_config->nr_vmtag == 0) {
		pr_error("No VM is found\n");
		return -ENOENT;
	}

	pr_debug("Found %d VMs config\n", mv_config->nr_vmtag);

	for (i = 0; i < mv_config->nr_vmtag; i++)
		mvisor_add_vm(&vmtags[i]);

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
		vcpu = (struct vcpu *)mvisor_malloc(sizeof(struct vcpu));
		if (vcpu == NULL)
			return -ENOMEM;

		memset((char *)vcpu, 0, sizeof(struct vcpu));

		vcpu->vcpu_id = i;
		vcpu->vm = vm;
		vcpu->entry_point = vm->entry_point;
		init_list(&vcpu->pcpu_list);
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

		vcpu_irq_struct_init(&vcpu->irq_struct);
	}

	return 0;
}

static void inline vm_sched_init(struct vm *vm)
{
	int i;
	struct vcpu *vcpu = NULL;

	for (i = 0; i < vm->vcpu_nr; i++) {
		vcpu = vm->vcpus[i];
		vcpu_sched_init(vcpu);
	}
}

static void inline vm_arch_init(struct vm *vm)
{
	arch_vm_init(vm);
}

static void inline vm_modules_init(struct vm *vm)
{
	int i;

	for (i = 0; i < vm->vcpu_nr; i++)
		vcpu_modules_init(vm->vcpus[i]);
}

void mvisor_boot_vms(void)
{
	int i;
	struct vcpu *vcpu;
	struct vm *vm;

	for_each_vm(vm) {
		for (i = 0; i < vm->vcpu_nr; i++) {
			vcpu = vm->vcpus[i];
			vm->os->ops->vcpu_init(vcpu);
		}
	}
}

int mvisor_vms_init(void)
{
	struct vm *vm;

	for_each_vm(vm) {
		vm->os = get_vm_os(vm->os_type);
		vm_memory_init(vm);
		vm_sched_init(vm);
		vm_arch_init(vm);
		vm_modules_init(vm);
	}

	return 0;
}

int mvisor_create_vms(void)
{
	struct vm *vm;

	if (parse_all_vms())
		panic("parsing the vm fail\n");

	for_each_vm(vm) {
		vm_mm_struct_init(vm);
		vm_create_vcpus(vm);
	}

	return 0;
}
