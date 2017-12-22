#include <core/core.h>
#include <core/vcpu.h>
#include <core/vm.h>

extern unsigned char __vmm_vm_start;
extern unsigned char __vmm_vm_end;

static vm_t *vms = NULL;
static uint32_t total_vms = 0;

phy_addr_t *vcpu_context_table = NULL;

static void init_vms_state(void)
{
	int i, j;
	vcpu_t *vcpu = NULL;
	vm_t *vm = NULL;

	/*
	 * find the boot cpu for each vm and
	 * mark its status, boot cpu will set
	 * to ready to run state then other vcpu
	 * is set to STOP state to wait for bootup
	 */
	for (i = 0; i < total_vms; i++) {
		vm = &vms[i];
		for (j = 0; j < vm->vcpu_nr; j++) {
			vcpu = vm->vcpus[j];
			if (get_vcpu_id(vcpu) == 0)
				set_vcpu_state(vcpu, VCPU_STATE_READY);
			else
				set_vcpu_state(vcpu, VCPU_STATE_STOP);
		}
	}
}

static int init_vcpu_context_table(int size)
{
	int total_size;

	total_size = size * CONFIG_VM_MAX_VCPU * sizeof(phy_addr_t *);
	vcpu_context_table = (phy_addr_t *)vmm_malloc(total_size);
	if (!vcpu_context_table)
		panic("No enought memory for vcpu_context_table\n");

	memset((char *)vcpu_context_table, 0, total_size);

	return 0;
}

int register_vcpu_context(phy_addr_t *context, uint32_t vmid, uint32_t vcpuid)
{
	if ((vmid > total_vms) || (vcpuid > CONFIG_VM_MAX_VCPU))
		return -EINVAL;

	vcpu_context_table[(vmid  * CONFIG_VM_MAX_VCPU) + vcpuid] = (uint64_t)context;
	return 0;
}

void init_vms(void)
{
	int i, j;
	vm_t *vm;
	vm_entry_t *vme;
	vcpu_t *vcpu;
	size_t size = (&__vmm_vm_end) - (&__vmm_vm_start);
	phy_addr_t *start = (phy_addr_t *)(&__vmm_vm_start);

	if (size == 0)
		panic("No VM is found\n");

	size = size / sizeof(vm_entry_t *);
	pr_debug("Found %d VMs config\n", size);

	vms = (vm_t *)vmm_malloc(size * sizeof(vm_t));
	if (NULL == vms)
		panic("No more memory for vms\n");

	memset((char *)vms, 0, size * sizeof(vm_t));
	total_vms = size;

	init_vcpu_context_table(size);

	for (i = 0; i < size; i++) {
		vme = (vm_entry_t *)(*start);
		vm = &vms[i];
		pr_info("found vm-%d %s ram_base:0x%x ram_size:0x%x nr_vcpu:%d\n",
			i, vme->name, vme->ram_base, vme->ram_size, vme->nr_vcpu);

		vm->vmid = i;
		strncpy(vm->name, vme->name,
			MIN(strlen(vme->name), VMM_VM_NAME_SIZE - 1));
		vm->ram_base = vme->ram_base;
		vm->ram_size = vme->ram_size;
		vm->vcpu_nr = MIN(vme->nr_vcpu, CONFIG_VM_MAX_VCPU);

		for (j = 0; j < vm->vcpu_nr; j++)
		{
			vcpu = create_vcpu(vm, j, vme->boot_vm,
					vme->vcpu_affinity[j],
					vme->entry_point);
			if (NULL == vcpu)
				panic("No more memory to create VCPU\n");
			vm->vcpus[j] = vcpu;
		}

		start++;
	}

	init_vms_state();
}
