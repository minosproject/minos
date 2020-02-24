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
#include <virt/vmm.h>
#include <virt/os.h>
#include <minos/sched.h>
#include <virt/vdev.h>
#include <minos/pm.h>
#include <minos/of.h>
#include <minos/vmodule.h>
#include <virt/resource.h>
#include <common/gvm.h>
#include <virt/vmcs.h>
#include <virt/vmbox.h>

extern void virqs_init(void);
extern void fdt_vm_init(struct vm *vm);

static int aff_current;
DECLARE_BITMAP(vcpu_aff_bitmap, NR_CPUS);
DEFINE_SPIN_LOCK(affinity_lock);

static int vcpu_affinity_init(void)
{
	int i;
	struct vm *vm;

	bitmap_clear(vcpu_aff_bitmap, 0, NR_CPUS);

	for_each_vm(vm) {
		for (i = 0; i < vm->vcpu_nr; i++)
			set_bit(vm->vcpu_affinity[i], vcpu_aff_bitmap);
	}

	aff_current = find_first_zero_bit(vcpu_aff_bitmap, NR_CPUS);

	return 0;
}

void get_vcpu_affinity(uint32_t *aff, int nr)
{
	int i = 0;
	int vm0_vcpu0_ok = 0;
	int vm0_vcpus_ok = 0;
	struct vm *vm0 = get_vm_by_id(0);
	int vm0_vcpu0 = vm0->vcpu_affinity[0];

	if (nr == NR_CPUS)
		vm0_vcpu0_ok = 1;
	else if (nr > (NR_CPUS - vm0->vcpu_nr))
		vm0_vcpus_ok = 1;

	spin_lock(&affinity_lock);

	do {
		if (!test_bit(aff_current, vcpu_aff_bitmap)) {
			aff[i] = aff_current;
			i++;
		} else {
			if ((aff_current == vm0_vcpu0) && vm0_vcpu0_ok) {
				aff[i] = aff_current;
				i++;
			} else if ((aff_current != vm0_vcpu0) && vm0_vcpus_ok) {
				aff[i] = aff_current;
				i++;
			}
		}

		if (++aff_current >= NR_CPUS)
			aff_current = 0;
	} while (i < nr);

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
	if (tag->flags & VM_FLAGS_DYNAMIC_AFF) {
		memset(tag->vcpu_affinity, 0, sizeof(tag->vcpu_affinity));
		get_vcpu_affinity(tag->vcpu_affinity, tag->nr_vcpu);
	}

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

	if (vm == NULL)
		return -ENOENT;

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
	preempt_disable();
	vm->state = VM_STAT_OFFLINE;

	/*
	 * just set all the vcpu of this vm to idle
	 * state, then send a virq to host to notify
	 * host that this vm need to be reset
	 */
	vm_for_each_vcpu(vm, vcpu) {
		ret = vcpu_power_off(vcpu, 1000);
		if (ret)
			pr_warn("power off vcpu-%d failed\n", vcpu->vcpu_id);
	}

	if (args == NULL) {
		pr_info("vm shutdown request by itself\n");
		trap_vcpu_nonblock(VMTRAP_TYPE_COMMON,
			VMTRAP_REASON_SHUTDOWN, 0, NULL);
		set_need_resched();
		preempt_enable();

		/* called by itself need to sched out */
		sched();
	} else
		preempt_enable();

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

static int guest_mm_init(struct vm *vm, uint64_t base, uint64_t size)
{
	if (split_vmm_area(&vm->mm, base, 0, size, VM_NORMAL | VM_MAP_BK)) {
		pr_err("invalid memory config for guest VM\n");
		return -EINVAL;
	}

	if (alloc_vm_memory(vm)) {
		pr_err("allocate memory for vm-%d failed\n", vm->vmid);
		return -ENOMEM;
	}

	return 0;
}

int create_vm_mmap(int vmid,  unsigned long offset,
		unsigned long size, unsigned long *addr)
{
	struct vm *vm = get_vm_by_id(vmid);
	struct vmm_area *va;

	va = vm_mmap(vm, offset, size);
	if (!va)
		return -EINVAL;

	*addr = va->start;
	return 0;
}

int create_guest_vm(struct vmtag *tag)
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

	ret = guest_mm_init(vm, vmtag->mem_base, vmtag->mem_size);
	if (ret)
		goto release_vm;

	ret = vm->vmid;
	goto unmap_vmtag;

release_vm:
	destroy_vm(vm);
unmap_vmtag:
	unmap_vm_mem((unsigned long)tag, sizeof(struct vmtag));
	return ret;
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
	preempt_disable();
	vm->state = VM_STAT_REBOOT;

	/*
	 * if the args is NULL, then this reset is requested by
	 * iteself, otherwise the reset is called by vm0
	 */
	vm_for_each_vcpu(vm, vcpu) {
		ret = vcpu_power_off(vcpu, 1000);
		if (ret) {
			pr_err("vm-%d vcpu-%d power off failed\n",
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
		set_need_resched();
		preempt_enable();

		/*
		 * no need to reset the vmodules for the vm, since
		* the state will be reset when power up, then sched
		* out if the reset is called by itself
		*/
		sched();
	} else
		preempt_enable();

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

		resume_task_vmodule_state(vcpu->task);
	}

	do_hooks((void *)vm, NULL, OS_HOOK_RESUME_VM);
	trap_vcpu_nonblock(VMTRAP_TYPE_COMMON,
			VMTRAP_REASON_VM_RESUMED, 0, NULL);

	return 0;
}

static int __vm_suspend(struct vm *vm)
{
	struct vcpu *vcpu = get_current_vcpu();

	pr_info("suspend vm-%d\n", vm->vmid);
	if (get_vcpu_id(vcpu) != 0) {
		pr_err("vm suspend can only called by vcpu0\n");
		return -EPERM;
	}

	vm_for_each_vcpu(vm, vcpu) {
		if (get_vcpu_id(vcpu) == 0)
			continue;

		if (vcpu->task->stat != TASK_STAT_STOPPED) {
			pr_err("vcpu-%d is not suspend vm suspend fail\n",
					get_vcpu_id(vcpu));
			return -EINVAL;
		}

		suspend_task_vmodule_state(vcpu->task);
	}

	vm->state = VM_STAT_SUSPEND;
	trap_vcpu_nonblock(VMTRAP_TYPE_COMMON,
			VMTRAP_REASON_VM_SUSPEND, 0, NULL);

	/* call the hooks for suspend */
	do_hooks((void *)vm, NULL, OS_HOOK_SUSPEND_VM);

	set_task_suspend(0);
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


int vm_create_host_vdev(struct vm *vm)
{
	phy_addr_t addr;

	/*
	 * convert the guest's memory to hypervisor's memory space
	 * do not need to map again, since all the guest VM's memory
	 * has been mapped when mm_init()
	 */
	addr = translate_vm_address(vm, (unsigned long)vm->setup_data);
	if (!addr)
		return -ENOMEM;

	return create_vm_resource_of(vm, (void *)addr);
}

static int vm_create_resource(struct vm *vm)
{
	/* 
	 * first map the dtb address to the hypervisor, here
	 * map these native VM's memory as read only
	 */
	create_host_mapping((vir_addr_t)vm->setup_data,
			(phy_addr_t)vm->setup_data, MEM_BLOCK_SIZE, VM_RO);

	if (of_data(vm->setup_data)) {
		vm->flags |= VM_FLAGS_SETUP_OF;
		create_vm_resource_of(vm, vm->setup_data);
	}

	destroy_host_mapping((vir_addr_t)vm->setup_data, MEM_BLOCK_SIZE);

	return 0;
}

static void setup_vm(struct vm *vm)
{
	if (vm->flags & VM_FLAGS_SETUP_OF)
		fdt_vm_init(vm);
}

static void *create_native_vm_of(struct device_node *node, void *arg)
{
	int ret, i;
	struct vm *vm;
	struct vmtag vmtag;
	uint64_t meminfo[2 * VM_MAX_MEM_REGIONS];

	if (node->class != DT_CLASS_VM)
		return NULL;

	ret = parse_vm_info_of(node, &vmtag);
	if (ret)
		return NULL;

	pr_info("**** create new vm ****\n");
	pr_info("    vmid: %d\n", vmtag.vmid);
	pr_info("    name: %s\n", vmtag.name);
	pr_info("    os_type: %s\n", vmtag.os_type);
	pr_info("    nr_vcpu: %d\n", vmtag.nr_vcpu);
	pr_info("    entry: 0x%p\n", vmtag.entry);
	pr_info("    setup_data: 0x%p\n", vmtag.setup_data);
	pr_info("    %s-bit vm\n", vmtag.flags & VM_FLAGS_64BIT ? "64" : "32");
	pr_info("    flags: 0x%x\n", vmtag.flags);
	pr_info("    affinity: %d %d %d %d %d %d %d %d\n",
			vmtag.vcpu_affinity[0], vmtag.vcpu_affinity[1],
			vmtag.vcpu_affinity[2], vmtag.vcpu_affinity[3],
			vmtag.vcpu_affinity[4], vmtag.vcpu_affinity[5],
			vmtag.vcpu_affinity[6], vmtag.vcpu_affinity[7]);

	vm = (void *)create_vm(&vmtag);
	if (!vm) {
		pr_err("create vm-%d failed\n", vmtag.vmid);
		return NULL;
	}

	/* parse the memory information of the vm from dtb */
	ret = of_get_u64_array(node, "memory", meminfo, 2 * VM_MAX_MEM_REGIONS);
	if ((ret <= 0) || ((ret % 2) != 0)) {
		pr_err("get wrong memory information for vm-%d", vmtag.vmid);
		destroy_vm(vm);

		return NULL;
	}

	ret = ret / 2;

	for (i = 0; i < ret; i ++) {
		split_vmm_area(&vm->mm, meminfo[i * 2], meminfo[i * 2],
				meminfo[i * 2 + 1], VM_NORMAL | VM_MAP_PT);
	}

	return vm;
}

static void parse_and_create_vms(void)
{
#ifdef CONFIG_DEVICE_TREE
	of_iterate_all_node_loop(hv_node, create_native_vm_of, NULL);
#endif
}

static int of_create_vmboxs(void)
{
	struct device_node *mailboxes;
	struct device_node *child;

	mailboxes = of_find_node_by_name(hv_node, "vmboxs");
	if (!mailboxes)
		return -ENOENT;

	/* parse each mailbox entry and create it */
	of_node_for_each_child(mailboxes, child) {
		if (of_create_vmbox(child))
			pr_err("create mailbox [%s] fail\n", child->name);
		else
			pr_info("create mailbox [%s] successful\n", child->name);
	}

	return 0;
}

int virt_init(void)
{
	struct vm *vm;

	virqs_init();

	/* parse the vm information from dtb */
	parse_and_create_vms();

	/* check whether VM0 has been create correctly */
	vm = get_vm_by_id(0);
	if (!vm) {
		pr_err("vm0 has not been create correctly\n");
		return -ENOENT;
	}

	vcpu_affinity_init();

#ifdef CONFIG_DEVICE_TREE
	/* here create all the mailbox for all native vm */
	of_create_vmboxs();
#endif

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
		vm_create_resource(vm);
		setup_vm(vm);
		vm_mm_init(vm);
		vm->state = VM_STAT_ONLINE;

		/* need after all the task of the vm setup is finished */
		vm_vcpus_init(vm);
	}

	return 0;
}
