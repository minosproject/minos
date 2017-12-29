#include <core/core.h>
#include <core/vm.h>
#include <core/vcpu.h>
#include <core/pcpu.h>
#include <asm/cpu.h>
#include <config/vm_config.h>

extern unsigned char __vmm_vm_start;
extern unsigned char __vmm_vm_end;

static vm_t *vms = NULL;
static uint32_t total_vms = 0;

struct list_head shared_mem_list;

#ifdef CONFIG_ARM_AARCH64

static int set_up_vcpu_env(vcpu_t *vcpu)
{
	vcpu_context_t *c = &vcpu->context;
	uint32_t vmid = get_vm_id(vcpu);
	uint32_t vcpu_id = get_vcpu_id(vcpu);
	uint32_t pcpu_id = get_pcpu_id(vcpu);

	c->x0 = 0;
	c->x1 = 1;
	c->x2 = 2;
	c->x3 = 3;
	c->x4 = 4;
	c->x5 = 5;
	c->x6 = 6;
	c->x7 = 7;
	c->x8 = 8;
	c->x9 = 9;
	c->x10 = 10;
	c->x11 = 11;
	c->x12 = 12;
	c->x13 = 13;
	c->x14 = 14;
	c->x15 = 15;
	c->x16 = 16;
	c->x17 = 17;
	c->x18 = 18;
	c->x19 = 19;
	c->x20 = 20;
	c->x21 = 21;
	c->x22 = 22;
	c->x23 = 23;
	c->x24 = 24;
	c->x25 = 25;
	c->x26 = 26;
	c->x27 = 27;
	c->x28 = 28;
	c->x29 = 29;
	c->x30_lr = 30;
	c->sp_el1 = 0x0;
	c->elr_el2 = vcpu->entry_point;
	c->vbar_el1 = 0;
	c->spsr_el1 = AARCH64_SPSR_EL1h | AARCH64_SPSR_F | \
		      AARCH64_SPSR_I | AARCH64_SPSR_A;
	c->nzcv = 0;
	c->esr_el1 = 0x0;
	c->vmpidr = generate_vcpu_id(vmid, vcpu_id, pcpu_id);

	return 0;
}

#else

static int set_up_vcpu_env(vcpu_t *vcpu)
{

}

#endif

vcpu_t *create_vcpu(vm_t *vm, int index, boot_vm_t func,
		uint32_t affinity, phy_addr_t entry_point)
{
	vcpu_t *vcpu;

	vcpu = (vcpu_t *)vmm_malloc(sizeof(vcpu_t));
	if (vcpu == NULL)
		return NULL;

	memset((char *)vcpu, 0, sizeof(vcpu_t));

	vcpu->vcpu_id = index;
	vcpu->vm_belong_to = vm;
	vcpu->entry_point = entry_point;
	vcpu->pcpu_affinity = pcpu_affinity(vcpu, affinity);
	if (vcpu->pcpu_affinity == PCPU_AFFINITY_FAIL) {
		pr_fatal("Can not affinity the vcpu %d to pcpu %d\n",
				vcpu->vcpu_id, affinity);
		panic(NULL);
	} else {
		pr_info("Affinity the vcpu %d to pcpu %d\n",
				vcpu->vcpu_id, vcpu->pcpu_affinity);
	}

	set_up_vcpu_env(vcpu);

	if (func)
		func(&vcpu->context);

	return vcpu;
}


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

static int parse_all_vms(void)
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

	for (i = 0; i < size; i++) {
		vme = (vm_entry_t *)(*start);
		vm = &vms[i];
		pr_info("found vm-%d %s nr_vcpu:%d\n", i, vme->name, vme->nr_vcpu);

		vm->vmid = vme->vmid;
		strncpy(vm->name, vme->name,
			MIN(strlen(vme->name), VMM_VM_NAME_SIZE - 1));
		vm->vcpu_nr = MIN(vme->nr_vcpu, CONFIG_VM_MAX_VCPU);
		init_list(&vm->mem_list);

		for (i = 0; i < total_vms; i++) {
			for (j = 0; j < vm->vcpu_nr; j++) {
				vcpu = create_vcpu(vm, j, vme->boot_vm,
					vme->vcpu_affinity[j],
					vme->entry_point);
				if (NULL == vcpu)
					panic("No more memory to create VCPU\n");

				vm->vcpus[j] = vcpu;
			}
		}


		start++;
	}
}

static int parse_vm_memory(void)
{
	uint32_t size;
	int i;
	struct vmm_memory_region *regions;
	struct vmm_memory_region *tmp;
	struct memory_region *m_reg;
	vm_t *vm;

	size = get_mem_config_size();
	regions = (struct vmm_memory_region *)get_mem_config_data();
	init_list(&shared_mem_list);

	if (size == 0)
		panic("Please get the memory config for system\n");

	for (i = 0; i < size; i++) {
		tmp = &regions[i];
		m_reg = (struct memory_region *)vmm_malloc(sizeof(struct memory_region));
		if (!m_reg)
			panic("No memory to parse the memory config");

		pr_debug("find memory region: 0x%x 0x%x %d %d %s\n",
				tmp->mem_base, tmp->mem_end, tmp->type,
				tmp->vmid, tmp->name);
		memset((char *)m_reg, 0, sizeof(struct memory_region));
		m_reg->mem_base = tmp->mem_base;
		m_reg->size = tmp->mem_end - tmp->mem_base;
		strncpy(m_reg->name, tmp->name,
			MIN(strlen(tmp->name), MEM_REGION_NAME_SIZE - 1));

		/*
		 * shared memory is for all vm to ipc
		 */
		if (tmp->type == 0x2) {
			m_reg->type = MEM_TYPE_NORMAL;
			list_add(&shared_mem_list, &m_reg->mem_region_list);
		} else {
			if (tmp->type == 0x0)
				m_reg->type = MEM_TYPE_NORMAL;
			else
				m_reg->type = MEM_TYPE_IO;

			vm = &vms[tmp->vmid];
		}
	}

	return 0;
}

static int map_vm_memory(void)
{
	int i;
	vm_t *vm;
	struct list_head *list;
	struct memory_region *region;
	phy_addr_t ttb2_addr;
	uint64_t tcr_el2;

	for (i = 0; i < total_vms; i++) {
		vm = &vms[i];
		ttb2_addr = mmu_map_vm_memory(&vm->mem_list);
		mmu_map_memory_region_list(ttb2_addr, &shared_mem_list);

		tcr_el2 = mmu_generate_tcr_el2();

		vm->ttb2_addr = ttb2_addr;
		vm->tcr_el2 = tcr_el2;
	}

	return 0;
}

uint64_t mmu_generate_tcr_el2(void)
{
	return 0;
}

int init_vms(void)
{
	int ret = 0;

	ret = parse_all_vms();
	if (ret)
		panic("parsing the vm fail\n");

	parse_vm_memory();
	map_vm_memory();
	init_vms_state();

	return 0;
}
