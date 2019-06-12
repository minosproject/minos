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
#include <minos/vm.h>
#include <minos/vcpu.h>
#include <minos/vmm.h>
#include <minos/os.h>
#include <minos/sched.h>
#include <minos/vdev.h>
#include <minos/pm.h>
#include <minos/of.h>
#include <minos/vmodule.h>
#include <minos/resource.h>
#include <common/gvm.h>

static struct vmtag *vmtags = NULL;
static int nr_static_vms;

extern void virqs_init(void);
extern void fdt_vm0_init(struct vm *vm);

static int e_base;
static int f_base;
static int e_base_current;
static int f_base_current;
static int e_nr;
static int f_nr;
DEFINE_SPIN_LOCK(affinity_lock);

void get_vcpu_affinity(uint32_t *aff, int nr)
{
	int i, e, f, j = 0;

	e = MIN(nr, e_nr);
	f = nr - e;

	spin_lock(&affinity_lock);

	for (i = 0; i < e; i++) {
		aff[j] = e_base_current;
		e_base_current++;
		if (e_base_current == NR_CPUS)
			e_base_current = e_base;
		j++;
	}

	for (i = 0; i < f; i++) {
		if ( (f_base_current == 0) && ((f < f_nr) || (i == 0)) ) {
			f_base_current++;
			if (f_base_current == e_base)
				f_base_current = f_base;
		}

		aff[j] = f_base_current;
		j++;
		f_base_current++;
		if (f_base_current == e_base)
			f_base_current = f_base;
	}

	spin_unlock(&affinity_lock);
}

static int vmtag_check_and_config(struct vmtag *tag)
{
	size_t size;

	/*
	 * first check whether there are enough memory for
	 * this vm and the vm's memory base need to be start
	 * at 0x80000000 or higher, if the mem_base is 0,
	 * then set it to default 0x80000000
	 */
	size = tag->mem_size;

	if (tag->mem_base == 0)
		tag->mem_base = GVM_NORMAL_MEM_START;

	if (tag->mem_base < GVM_NORMAL_MEM_START)
		return -EINVAL;

	if ((tag->mem_base + size) >= GVM_NORMAL_MEM_END)
		return -EINVAL;;

	if (!has_enough_memory(size))
		return -EINVAL;

	if (tag->nr_vcpu > NR_CPUS)
		return -EINVAL;

	/* for the dynamic need to get the affinity dynamicly */
	if (tag->flags & VM_FLAGS_DYNAMIC_AFF)
		get_vcpu_affinity(tag->vcpu_affinity, tag->nr_vcpu);

	return 0;
}

int request_vm_virqs(struct vm *vm, int base, int nr)
{
	if (!vm || (base < GVM_IRQ_BASE) || (nr <= 0) ||
			(base + nr >= GVM_IRQ_END))
		return -EINVAL;

	while (nr > 0) {
		request_virq(vm, base, 0);
		base++;
		nr--;
	}

	return 0;
}

int vm_power_up(int vmid)
{
	struct vm *vm = get_vm_by_id(vmid);

	if (vm == NULL) {
		return -ENOENT;
	}

	vm_vcpus_init(vm);
	vm->state = VM_STAT_ONLINE;

	return 0;
}

static int __vm_power_off(struct vm *vm, void *args)
{
	int ret = 0;
	struct vcpu *vcpu;

	if (vm_is_hvm(vm))
		panic("hvm can not call power_off_vm\n");

	/* set the vm to offline state */
	pr_info("power off vm-%d\n", vm->vmid);
	vm->state = VM_STAT_OFFLINE;

	/*
	 * just set all the vcpu of this vm to idle
	 * state, then send a virq to host to notify
	 * host that this vm need to be reset
	 */
	vm_for_each_vcpu(vm, vcpu) {
		ret = vcpu_power_off(vcpu, 1000);
		if (ret)
			pr_warn("power off vcpu-%d failed\n",
					vcpu->vcpu_id);
		pcpu_remove_vcpu(vcpu->affinity, vcpu);
	}

	if (args == NULL) {
		pr_info("vm shutdown request by itself\n");
		trap_vcpu_nonblock(VMTRAP_TYPE_COMMON,
			VMTRAP_REASON_SHUTDOWN, 0, NULL);

		/* called by itself need to sched out */
		sched();
	}

	return 0;
}

int vm_power_off(int vmid, void *arg)
{
	struct vm *vm = NULL;

	if (vmid == 0)
		system_shutdown();

	vm = get_vm_by_id(vmid);
	if (!vm)
		return -EINVAL;

	return __vm_power_off(vm, arg);
}

int create_new_vm(struct vmtag *tag)
{
	int ret;
	struct vm *vm;
	struct vmtag *vmtag;

	vmtag = (struct vmtag *)map_vm_mem((unsigned long)tag,
			sizeof(struct vmtag));
	if (!vmtag)
		return VMID_INVALID;

	ret = vmtag_check_and_config(vmtag);
	if (ret)
		goto unmap_vmtag;

	vm = create_vm(vmtag);
	if (!vm)
		goto unmap_vmtag;

	/*
	 * allocate memory to this vm
	 */
	ret = vm_mmap_init(vm, vmtag->mem_size);
	if (ret) {
		pr_error("no more mmap space for vm\n");
		goto release_vm;
	}

	vmtag->mmap_base = vm->mm.hvm_mmap_base;

	ret = alloc_vm_memory(vm, vmtag->mem_base, vmtag->mem_size);
	if (ret)
		goto release_vm;

	dsb();
	unmap_vm_mem((unsigned long)tag, sizeof(struct vmtag));

	return (vm->vmid);

release_vm:
	destroy_vm(vm);
unmap_vmtag:
	unmap_vm_mem((unsigned long)tag, sizeof(struct vmtag));

	return -ENOMEM;
}

static int __vm_reset(struct vm *vm, void *args)
{
	int ret;
	struct vdev *vdev;
	struct vcpu *vcpu;

	if (vm_is_hvm(vm))
		panic("hvm can not call reset vm\n");

	/* set the vm to offline state */
	pr_info("reset vm-%d\n", vm->vmid);
	vm->state = VM_STAT_REBOOT;

	/*
	 * if the args is NULL, then this reset is requested by
	 * iteself, otherwise the reset is called by vm0
	 */
	vm_for_each_vcpu(vm, vcpu) {
		ret = vcpu_power_off(vcpu, 1000);
		if (ret) {
			pr_error("vm-%d vcpu-%d power off failed\n",
					vm->vmid, vcpu->vcpu_id);
			return ret;
		}

		/* the vcpu is powered off, then reset it */
		ret = vcpu_reset(vcpu);
		if (ret)
			return ret;
	}

	/* reset the vdev for this vm */
	list_for_each_entry(vdev, &vm->vdev_list, list) {
		if (vdev->reset)
			vdev->reset(vdev);
	}

	vm_virq_reset(vm);

	if (args == NULL) {
		pr_info("vm reset trigger by itself\n");
		trap_vcpu_nonblock(VMTRAP_TYPE_COMMON,
			VMTRAP_REASON_REBOOT, 0, NULL);
		/*
		 * no need to reset the vmodules for the vm, since
		* the state will be reset when power up, then sched
		* out if the reset is called by itself
		*/
		sched();
	}

	return 0;
}

int vm_reset(int vmid, void *args)
{
	struct vm *vm;

	/*
	 * if the vmid is 0, means the host request a
	 * hardware reset
	 */
	if (vmid == 0)
		system_reboot();

	vm = get_vm_by_id(vmid);
	if (!vm)
		return -ENOENT;

	return __vm_reset(vm, args);
}

static int vm_resume(struct vm *vm)
{
	struct vcpu *vcpu;

	pr_info("vm-%d resumed\n", vm->vmid);

	vm_for_each_vcpu(vm, vcpu) {
		if (get_vcpu_id(vcpu) == 0)
			continue;

		resume_vcpu_vmodule_state(vcpu);
	}

	do_hooks((void *)vm, NULL, MINOS_HOOK_TYPE_RESUME_VM);
	trap_vcpu_nonblock(VMTRAP_TYPE_COMMON,
			VMTRAP_REASON_VM_RESUMED, 0, NULL);

	return 0;
}

static int __vm_suspend(struct vm *vm)
{
	struct vcpu *vcpu = get_current_vcpu();

	pr_info("suspend vm-%d\n", vm->vmid);
	if (get_vcpu_id(vcpu) != 0) {
		pr_error("vm suspend can only called by vcpu0\n");
		return -EPERM;
	}

	vm_for_each_vcpu(vm, vcpu) {
		if (get_vcpu_id(vcpu) == 0)
			continue;

		if (vcpu->state != VCPU_STAT_STOPPED) {
			pr_error("vcpu-%d is not suspend vm suspend fail\n",
					get_vcpu_id(vcpu));
			return -EINVAL;
		}

		suspend_vcpu_vmodule_state(vcpu);
	}

	vm->state = VM_STAT_SUSPEND;
	trap_vcpu_nonblock(VMTRAP_TYPE_COMMON,
			VMTRAP_REASON_VM_SUSPEND, 0, NULL);

	/* call the hooks for suspend */
	do_hooks((void *)vm, NULL, MINOS_HOOK_TYPE_SUSPEND_VM);

	set_vcpu_suspend(get_current_vcpu());
	sched();

	/* vm is resumed */
	vm->state = VM_STAT_ONLINE;
	vm_resume(vm);

	return 0;
}

int vm_suspend(int vmid)
{
	struct vm *vm = get_vm_by_id(vmid);

	if (vm == NULL) {
		return -EINVAL;
	}

	if (vm_is_hvm(vm))
		return system_suspend();

	return __vm_suspend(vm);
}

void set_vmtags_to(struct vmtag *tags, int count)
{
	if (!tags || (count == 0))
		panic("incorrect vmtags or count %d\n", count);

	vmtags = tags;
	nr_static_vms = count;
}

int vm_create_host_vdev(struct vm *vm)
{
	phy_addr_t addr;
	int ret;

	/*
	 * map the memory of the vm's setup data to
	 * the hypervisor's memory space, the setup
	 * data must 2M algin
	 */
	addr = get_vm_memblock_address(vm, (unsigned long)vm->setup_data);
	if (!addr)
		return -ENOMEM;

	ret = create_host_mapping(addr, addr, MEM_BLOCK_SIZE, VM_RO);
	if (ret)
		goto out;

	ret = create_vm_resource_of(vm, (void *)addr);
	destroy_host_mapping(addr, MEM_BLOCK_SIZE);

out:
	return ret;
}

static int vm_create_resource(struct vm *vm)
{
	if (of_data(vm->setup_data)) {
		vm->flags |= VM_FLAGS_SETUP_OF;
		return create_vm_resource_of(vm, vm->setup_data);
	}

	return -EINVAL;
}

static void setup_hvm(struct vm *vm)
{
	if (vm->flags & VM_FLAGS_SETUP_OF)
		fdt_vm0_init(vm);
}

int virt_init(void)
{
	int i;
	struct vm *vm;

	if ((vmtags == NULL) || (nr_static_vms == 0))
		panic("no vm config found\n");

	vmodules_init();
	virqs_init();

	for (i = 0; i < nr_static_vms; i++) {
		vm = create_vm(&vmtags[i]);
		if (!vm)
			pr_error("create VM(%d):%s failed\n", i,
				 vmtags[i].name);
	}

	/* check whether VM0 has been create correctly */
	vm = get_vm_by_id(0);
	if (!vm)
		panic("vm0 has not been create correctly\n");

	/*
	 * parsing all the memory/irq and resource
	 * from the setup data and create the resource
	 * for the vm
	 */
	for_each_vm(vm) {
		/*
		 * - map the vm's memory
		 * - create the vcpu for vm's each vcpu
		 * - init the vmodule state for each vcpu
		 * - prepare the vcpu for bootup
		 */
		vm_mm_init(vm);
		vm_create_resource(vm);
		vm_vcpus_init(vm);

		if (vm->vmid == 0)
			setup_hvm(vm);

		vm->state = VM_STAT_ONLINE;
	}

	return 0;
}

static int vcpu_affinity_init(void)
{
	struct vm *vm0 = get_vm_by_id(0);

	if (!vm0)
		panic("vm0 is not created\n");

	e_base_current = e_base = vm0->vcpu_nr;
	e_nr = NR_CPUS - vm0->vcpu_nr;
	f_base_current = f_base = 0;
	f_nr = vm0->vcpu_nr;

	return 0;
}
device_initcall(vcpu_affinity_init);
