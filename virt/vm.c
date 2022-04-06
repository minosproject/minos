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
#include <virt/os.h>
#include <virt/vm.h>
#include <virt/vmodule.h>
#include <virt/virq.h>
#include <virt/vmm.h>
#include <virt/vdev.h>
#include <virt/vmcs.h>
#include <minos/task.h>
#include <minos/pm.h>
#include <minos/of.h>
#include <virt/resource.h>
#include <uapi/gvm.h>
#include <virt/vmbox.h>
#include <minos/shell_command.h>
#include <virt/virt.h>
#include <minos/ramdisk.h>
#include <virt/iommu.h>
#include <asm/cache.h>

struct vm *vms[CONFIG_MAX_VM];
static int total_vms = 0;
LIST_HEAD(vm_list);

static struct vm *host_vm;

static DEFINE_SPIN_LOCK(vms_lock);
static DECLARE_BITMAP(vmid_bitmap, CONFIG_MAX_VM);

static int aff_current;
static int native_vcpus;
DECLARE_BITMAP(vcpu_aff_bitmap, NR_CPUS);
DEFINE_SPIN_LOCK(affinity_lock);

#define VM_NR_CPUS_CLUSTER 256

static void vcpu_online(struct vcpu *vcpu)
{
	ASSERT(check_vcpu_state(vcpu, VCPU_STATE_STOP));
	task_ready(vcpu->task, 0);
}

static int inline affinity_to_vcpuid(struct vm *vm, unsigned long affinity)
{
	int aff0, aff1;

	/*
	 * how to handle big-little soc ? usually the hvm's
	 * cpu map is as same as the true hardware, so here
	 * if the VM is the VM0, the affinity is as same as
	 * the real hardware.
	 *
	 * Can be different with real hardware ? TBD.
	 */
	if (vm_is_host_vm(vm))
		return affinity_to_cpuid(affinity);

	aff1 = (affinity >> 8) & 0xff;
	aff0 = affinity & 0xff;

	return (aff1 * VM_NR_CPUS_CLUSTER) + aff0;
}

int vcpu_power_on(struct vcpu *caller, unsigned long affinity,
		unsigned long entry, unsigned long unsed)
{
	int cpuid;
	struct vcpu *vcpu;

	cpuid = affinity_to_vcpuid(caller->vm, affinity);

	vcpu = get_vcpu_in_vm(caller->vm, cpuid);
	if (!vcpu) {
		pr_err("no such:%d->0x%x vcpu for this VM %s\n",
				cpuid, affinity, caller->vm->name);
		return -ENOENT;
	}

	if (check_vcpu_state(vcpu, TASK_STATE_STOP)) {
		pr_notice("vcpu-%d of vm-%d power on from vm suspend 0x%p\n",
				vcpu->vcpu_id, vcpu->vm->vmid, entry);
		os_vcpu_power_on(vcpu, ULONG(entry));
		vcpu_online(vcpu);
	} else {
		pr_err("vcpu_power_on : invalid vcpu state\n");
		return -EINVAL;
	}

	return 0;
}

void vcpu_context_save(struct task *task)
{
	save_vcpu_vmodule_state(task_to_vcpu(task));
}

void vcpu_context_restore(struct task *task)
{
	restore_vcpu_vmodule_state(task_to_vcpu(task));
}

int vcpu_can_idle(struct vcpu *vcpu)
{
	if (vcpu_has_irq(vcpu))
		return 0;

	return 1;
}

int vcpu_idle(struct vcpu *vcpu)
{
	return wait_event(&vcpu->vcpu_event,
			vcpu_can_idle(vcpu), 0);
}

int vcpu_suspend(struct vcpu *vcpu, gp_regs *c,
		uint32_t state, unsigned long entry)
{
	/*
	 * just call vcpu idle to put vcpu to suspend state
	 * and ignore the wake up entry, since the vcpu will
	 * not really powered off
	 */
	return vcpu_idle(vcpu);
}

int vcpu_off(struct vcpu *vcpu)
{
	task_need_freeze(vcpu->task);
	return 0;
}

static int vm_check_vcpu_affinity(int vmid, uint32_t *aff, int nr)
{
	int i;
	uint64_t mask = 0;

	for (i = 0; i < nr; i++) {
		if (aff[i] >= VM_MAX_VCPU)
			return -EINVAL;

		if (mask & (1 << aff[i]))
			return -EINVAL;
		else
			mask |= (1 << aff[i]);
	}

	return 0;
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

static inline void kick_by_hirq(struct vcpu *vcpu, int mode, int ret)
{
	/*
	 * ret < 0 means the task do not need to wake up and in ready state.
	 * if task is in ready state, or the task is running in guest mode
	 * then need send a physical irq to the target irq.
	 */
	if ((mode == IN_GUEST_MODE) || (mode == OUTSIDE_ROOT_MODE) || (ret < 0))
		pcpu_resched(vcpu_affinity(vcpu));
}

static inline void kick_by_virq(struct vcpu *vcpu, int mode, int ret)
{
	/*
	 * if the virq is not hardware virq, when the native
	 * wfi is enabled for the target vcpu, the target vcpu
	 * may not receive the virq immediately, and may wait
	 * last physical irq come, then this pcpu can wakeup
	 * from the WFI mode, so here need to send a phyical
	 * irq to the target pcpu. Native WFI VCPU will always
	 * in running mode in EL1.
	 */
	if ((vcpu->vm->flags & VM_FLAGS_NATIVE_WFI))
		pcpu_resched(vcpu_affinity(vcpu));
}

int kick_vcpu(struct vcpu *vcpu, int reason)
{
	int vcpu_mode, ret = 0;
	int same_cpu;

	vcpu_mode = vcpu->mode;
	smp_rmb();
	same_cpu = (smp_processor_id() == vcpu_affinity(vcpu));

	/*
	 * 1 - whether need to wake up the task.
	 * 2 - whether need to send a resched irq.
	 *
	 * if the vcpu is in stop state, only the BootCPU
	 * can wake up it. this will be another path.
	 */
	ret = wake(&vcpu->vcpu_event);

	/*
	 * 0   - wakeup successfuly
	 * 1   - do not need wake up task
	 * < 0 - wake up failed
	 * if on the same cpu, just call cond_resched to
	 * see whether need preempt this task.
	 */
	if (same_cpu) {
		cond_resched();
		return ret;
	}

	switch (reason) {
	case VCPU_KICK_REASON_VIRQ:
		kick_by_virq(vcpu, vcpu_mode, ret);
		break;
	case VCPU_KICK_REASON_HIRQ:
		kick_by_hirq(vcpu, vcpu_mode, ret);
		break;
	default:
		/*
		 * do nothing ?
		 */
		break;
	}

	return ret;
}

static void inline release_vcpu(struct vcpu *vcpu)
{
	/*
	 * need to make sure that when free the memory resource
	 * can not be done in the interrupt context, so the
	 * destroy a task will done in the idle task, here
	 * just call vmodule_stop call back, then set the
	 * task to the stop list of the pcpu, when the idle
	 * task is run, the idle task will release this task
	 */
	if (vcpu->context)
		stop_vcpu_vmodule_state(vcpu);

	if (vcpu->task)
		do_release_task(vcpu->task);

	if (vcpu->vmcs_irq >= 0)
		release_hvm_virq(vcpu->vmcs_irq);

	free(vcpu->virq_struct);
	free(vcpu);
}

static struct vcpu *alloc_vcpu(void)
{
	struct vcpu *vcpu;

	vcpu = zalloc(sizeof(*vcpu));
	if (!vcpu)
		return NULL;

	vcpu->virq_struct = zalloc(sizeof(struct virq_struct));
	if (!vcpu->virq_struct)
		goto free_vcpu;

	vcpu->vmcs_irq = -1;
	return vcpu;

free_vcpu:
	free(vcpu);

	return NULL;
}

static void vcpu_return_to_user(struct task *task, gp_regs *regs)
{
	struct vcpu *vcpu = (struct vcpu *)task->pdata;

	vcpu->mode = OUTSIDE_ROOT_MODE;
	smp_wmb();

	do_hooks(vcpu, (void *)regs, OS_HOOK_ENTER_TO_GUEST);

	vcpu->mode = IN_ROOT_MODE;
	smp_wmb();
}

static void vcpu_exit_from_user(struct task *task, gp_regs *regs)
{
	struct vcpu *vcpu = (struct vcpu *)task->pdata;

	vcpu->mode = OUTSIDE_GUEST_MODE;
	smp_wmb();

	do_hooks(vcpu, (void *)regs, OS_HOOK_EXIT_FROM_GUEST);

	vcpu->mode = IN_ROOT_MODE;
	smp_wmb();
}

static struct vcpu *create_vcpu(struct vm *vm, uint32_t vcpu_id)
{
	char name[64];
	struct vcpu *vcpu;
	struct task *task;

	/* generate the name of the vcpu task */
	memset(name, 0, 64);
	sprintf(name, "%s-vcpu-%d", vm->name, vcpu_id);
	task = create_vcpu_task(name, vm->entry_point,
			vm->vcpu_affinity[vcpu_id], 0, NULL);
	if (task == NULL)
		return NULL;

	task->return_to_user = vcpu_return_to_user;
	task->exit_from_user = vcpu_exit_from_user;

	vcpu = alloc_vcpu();
	if (!vcpu) {
		do_release_task(task);
		return NULL;
	}

	task->pdata = vcpu;
	vcpu->task = task;
	vcpu->vcpu_id = vcpu_id;
	vcpu->vm = vm;
	vcpu->mode = IN_ROOT_MODE;

	if (vm->flags & VM_FLAGS_32BIT)
		task->flags |= TASK_FLAGS_32BIT;

	init_list(&vcpu->list);

	vcpu_virq_struct_init(vcpu);
	vm->vcpus[vcpu_id] = vcpu;
	event_init(&vcpu->vcpu_event, OS_EVENT_TYPE_NORMAL, NULL);

	vcpu->next = NULL;
	if (vcpu_id != 0)
		vm->vcpus[vcpu_id - 1]->next = vcpu;

	return vcpu;
}

void vcpu_reset(struct vcpu *vcpu)
{
	reset_vcpu_vmodule_state(vcpu);
	vcpu_virq_struct_reset(vcpu);
}

static void vcpu_power_off_call(void *data)
{
	struct vcpu *vcpu = (struct vcpu *)data;
	
	if (current != vcpu->task)
		set_need_resched();
}

void vcpu_enter_poweroff(struct vcpu *vcpu)
{
	struct task *task = vcpu->task;
	int skip = 0;

	if (check_vcpu_state(vcpu, VCPU_STATE_STOP) ||
			check_vcpu_state(vcpu, VCPU_STATE_SUSPEND))
		return;

	task_need_freeze(task);
	if (vcpu->task == current)
		return;

	wake_up_abort(task);

	if ((vcpu_affinity(vcpu) != smp_processor_id()) && !skip) {
		pr_debug("call vcpu_power_off_call for vcpu-%s\n",
				vcpu->task->name);
		smp_function_call(vcpu->task->affinity,
				vcpu_power_off_call, (void *)vcpu, 0);
	}
}

static int alloc_new_vmid(void)
{
	int vmid, start = total_vms;

	spin_lock(&vms_lock);
	vmid = find_next_zero_bit_loop(vmid_bitmap, CONFIG_MAX_VM, start);
	if (vmid >= CONFIG_MAX_VM) {
		vmid = 0;
		goto out;
	}

	set_bit(vmid, vmid_bitmap);
out:
	spin_unlock(&vms_lock);

	return vmid;
}

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
	if (aff_current >= NR_CPUS)
		aff_current = (NR_CPUS - 1);

	for (i = 0; i < NR_CPUS; i++) {
		if (test_bit(i, vcpu_aff_bitmap))
			native_vcpus++;
	}

	return 0;
}

void get_vcpu_affinity(uint32_t *aff, int nr)
{
	int i = 0;
	int vm0_vcpu0_ok = 0;
	int vm0_vcpus_ok = 0;
	struct vm *vm0 = get_host_vm();
	int vm0_vcpu0 = vm0->vcpu_affinity[0];

	if (nr == NR_CPUS)
		vm0_vcpu0_ok = 1;
	else if (nr > (NR_CPUS - native_vcpus))
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

	if (!vmm_has_enough_memory(size)) {
		pr_err("no enough memory for guest\n");
		return -ENOMEM;
	}

	if (tag->nr_vcpu > NR_CPUS) {
		pr_err("to much vcpus for guest\n");
		return -EINVAL;
	}

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

static int load_vm_image(struct vm *vm)
{
	void *addr = (void *)ptov(vm->load_address);
	size_t size;
	int ret;

	if (!vm->kernel_file)
		return 0;

	pr_notice("copying %s to 0x%x\n", ramdisk_file_name(vm->kernel_file),
			vm->load_address);

	size = ramdisk_file_size(vm->kernel_file);
	ret = create_host_mapping(ULONG(addr), ULONG(vm->load_address),
			PAGE_BALIGN(size), VM_NORMAL | VM_HUGE);
	ASSERT(ret == 0);

	ret = ramdisk_read(vm->kernel_file, addr, size, 0);
	ASSERT(ret == 0);

	flush_dcache_range(ULONG(addr), PAGE_BALIGN(size));
	destroy_host_mapping(ULONG(addr), PAGE_BALIGN(size));

	return 0;
}

static void start_vm(struct vm *vm)
{
	struct vcpu *vcpu0;

	if (!vm) {
		pr_err("no such vm\n");
		return;
	}

	if (vm->state == VM_STATE_ONLINE) {
		pr_err("VM %s already stared\n", vm->name);
		return;
	}

	vcpu0 = vm->vcpus[0];
	if (!vcpu0) {
		pr_err("VM create with error, vm%d not exist\n", vm->vmid);
		return;
	}

	vm->state = VM_STATE_ONLINE;
	vcpu_online(vcpu0);
}

int vm_power_up(int vmid)
{
	struct vm *vm = get_vm_by_id(vmid);

	if (vm == NULL)
		return -ENOENT;

	vm_vcpus_init(vm);
	vm->state = VM_STATE_ONLINE;

	/*
	 * start the vm now
	 */
	start_vm(vm);

	return 0;
}

static int __vm_power_off(struct vm *vm, void *args, int byself)
{
	struct vcpu *vcpu;

	/*
	 * natvie VM do not support powered off by other.
	 */
	ASSERT(!byself && !vm_is_native(vm));

	pr_notice("power off vm-%d by %s\n", vm->vmid,
			byself ? "itself" : "mvm");

	set_vm_state(vm, VM_STATE_OFFLINE);
	vm_for_each_vcpu(vm, vcpu) {
		if (vcpu == current_vcpu)
			continue;
		vcpu_enter_poweroff(vcpu);
	}

	/*
	 * the vcpu has been set to TIF_NEED_STOP, so when return
	 * to guest, the task will be killed by kernel.
	 */
	if (!vm_is_native(vm) && byself) {
		pr_notice("send SHUTDOWN request to mvm\n");
		trap_vcpu_nonblock(VMTRAP_TYPE_COMMON,
				VMTRAP_REASON_SHUTDOWN, 0, NULL);
	} else {
		pr_notice("send SHUTDOWN request to VM daemon\n");
		send_vm_shutdown_request(vm);
	}

	return 0;
}

int vm_power_off(int vmid, void *arg, int byself)
{
	struct vm *vm = NULL;

	vm = get_vm_by_id(vmid);
	if (!vm)
		return -EINVAL;

	return __vm_power_off(vm, arg, byself);
}

static int guest_mm_init(struct vm *vm, uint64_t base, uint64_t size)
{
	if (split_vmm_area(&vm->mm, base, size, VM_GUEST_NORMAL) == NULL) {
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

int create_guest_vm(struct vmtag __guest *tag)
{
	int ret = 0;
	struct vm *vm;
	struct vmtag vmtag;

	memset(&vmtag, 0, sizeof(struct vmtag));
	ret = copy_from_guest(&vmtag, tag, sizeof(struct vmtag));
	if (ret != 0) {
		pr_err("copy vmtag from guest failed\n");
		return -EFAULT;
	}

	ret = vmtag_check_and_config(&vmtag);
	if (ret)
		return ret;

	vmtag.vmid = 0;
	vmtag.flags |= VM_FLAGS_CAN_RESET;
	vm = create_vm(&vmtag);
	if (!vm)
		return -ENOMEM;

	ret = guest_mm_init(vm, vmtag.mem_base, vmtag.mem_size);
	if (ret) {
		destroy_vm(vm);
		return ret;
	}

	return vm->vmid;
}

static int __vm_reset(struct vm *vm, void *args, int byself)
{
	struct vcpu *vcpu;

	pr_notice("reset vm-%d by %s\n",
			vm->vmid, byself ? "itself" : "mvm");
	if (vm_is_native(vm) && !(vm->flags & VM_FLAGS_CAN_RESET)) {
		pr_err("vm%d do not support reset\n", vm->vmid);
		return -EPERM;
	}

	/*
	 * if the args is NULL, then this reset is requested by
	 * iteself, otherwise the reset is called by vm0
	 */
	if (args == NULL)
		byself = 1;

	set_vm_state(vm, VM_STATE_REBOOT);
	vm_for_each_vcpu(vm, vcpu)
		vcpu_enter_poweroff(vcpu);

	if (!vm_is_native(vm) && byself) {
		pr_notice("send REBOOT request to mvm\n");
		trap_vcpu_nonblock(VMTRAP_TYPE_COMMON,
				VMTRAP_REASON_REBOOT, 0, NULL);
	} else {
		pr_notice("send REBOOT request to VM daemon\n");
		send_vm_reboot_request(vm);
	}

	return 0;
}

int vm_reset(int vmid, void *args, int byself)
{
	struct vm *vm;

	vm = get_vm_by_id(vmid);
	if (!vm)
		return -ENOENT;

	return __vm_reset(vm, args, byself);
}

static int vm_resume(struct vm *vm)
{
	struct vcpu *vcpu;

	pr_notice("vm-%d resumed\n", vm->vmid);

	vm_for_each_vcpu(vm, vcpu) {
		if (get_vcpu_id(vcpu) == 0)
			continue;

		resume_vcpu_vmodule_state(vcpu);
	}

	do_hooks((void *)vm, NULL, OS_HOOK_RESUME_VM);

	if (!vm_is_native(vm)) {
		pr_notice("send VM RESUME request to mvm\n");
		trap_vcpu_nonblock(VMTRAP_TYPE_COMMON,
				VMTRAP_REASON_VM_RESUMED, 0, NULL);
	}

	return 0;
}

static int __vm_suspend(struct vm *vm)
{
	struct vcpu *vcpu;

	pr_notice("suspend vm-%d\n", vm->vmid);
	if (get_vcpu_id(current_vcpu) != 0) {
		pr_err("vm suspend can only called by vcpu0\n");
		return -EPERM;
	}

	vm_for_each_vcpu(vm, vcpu) {
		if (vcpu == current_vcpu)
			continue;

		if (!check_vcpu_state(vcpu, TASK_STATE_STOP)) {
			pr_err("vcpu-%d is not suspend vm suspend fail\n",
					get_vcpu_id(vcpu));
			return -EINVAL;
		}

		/*
		 * other VCPU will powered up by vcpu0 again when
		 * it is suspended.
		 */
		suspend_vcpu_vmodule_state(vcpu);
	}

	vm->state = VM_STATE_SUSPEND;
	smp_mb();

	do_hooks((void *)vm, NULL, OS_HOOK_SUSPEND_VM);

	if (!vm_is_native(vm)) {
		pr_notice("send VM SUSPEND request to mvm\n");
		trap_vcpu_nonblock(VMTRAP_TYPE_COMMON,
				VMTRAP_REASON_VM_SUSPEND, 0, NULL);
	}

	/*
	 * vcpu0 will set to WAIT_EVENT state, so the interrupt
	 * can wakeup it, other VCPU will set to STOP state. Only
	 * can be wake up by vcpu0.
	 */
	vcpu_idle(current_vcpu);

	/*
	 * vm is resumed
	 */
	vm->state = VM_STATE_ONLINE;
	smp_wmb();

	vm_resume(vm);

	return 0;
}

int vm_suspend(int vmid)
{
	struct vm *vm = get_vm_by_id(vmid);

	if (vm == NULL)
		return -EINVAL;

	return __vm_suspend(vm);
}

static void do_setup_native_vm(struct vm *vm)
{
	/* 
	 * here need to create the resource based on the vm's
	 * os, when the os is a linux system, usually it will
	 * used device tree, if the os is rtos, need to write
	 * the iomem and virqs in the hypervisor's device tree
	 *
	 * here to check whether there are information in the
	 * hypervisor's dts, if not then try to parsing the dtb
	 * of the VM
	 *
	 * first map the dtb address to the hypervisor, here
	 * map these native VM's memory as read only
	 */
	os_create_native_vm_resource(vm);

	create_vmbox_controller(vm);

	os_setup_vm(vm);

	do_hooks(vm, NULL, OS_HOOK_SETUP_VM);
}

static void setup_native_vm(struct vm *vm)
{
	void *setup_addr = (void *)ptov(vm->setup_data);
	size_t size;
	int ret;

	/*
	 * first load the setup data from the ramdisk if needed.
	 * the setup data ususally is device tree on ARM, need
	 * map the memory into hypervisor's space. The memory
	 * of setup data can not beyond 2M.
	 */
	if (vm->dtb_file) {
		pr_notice("copying %s to 0x%x\n", ramdisk_file_name(vm->dtb_file),
				vm->setup_data);
		size = ramdisk_file_size(vm->dtb_file);
		ret = create_host_mapping(ULONG(setup_addr), ULONG(vm->setup_data),
				MAX_DTB_SIZE, VM_NORMAL | VM_HUGE);
		ASSERT(ret == 0);

		ret = ramdisk_read(vm->dtb_file, setup_addr, size, 0);
		ASSERT(ret == 0);
	} else {
		ret = create_host_mapping(ULONG(setup_addr), ULONG(vm->setup_data),
				MAX_DTB_SIZE, VM_NORMAL | VM_HUGE);
		ASSERT(ret == 0);
	}

	do_setup_native_vm(vm);

	/*
	 * the DTB content may modified, get the final size, and
	 * then flush the cache and unmap the memory.
	 */
	size = fdt_totalsize(setup_addr);
	flush_dcache_range(ULONG(setup_addr), PAGE_BALIGN(size));
	destroy_host_mapping(ULONG(setup_addr), MAX_DTB_SIZE);
}

void destroy_vm(struct vm *vm)
{
	int i;
	unsigned long flags;
	struct vdev *vdev, *n;
	struct vcpu *vcpu;

	if (!vm)
		return;

	if (vm_is_native(vm))
		panic("can not destory native VM\n");

	/*
	 * 1 : release the vdev
	 * 2 : do hooks for each modules
	 * 3 : release the vcpu allocated to this vm
	 * 4 : free the memory for this VM
	 * 5 : update the vmid bitmap
	 * 6 : do vmodule deinit
	 */
	list_for_each_entry_safe(vdev, n, &vm->vdev_list, list) {
		list_del(&vdev->list);
		if (vdev->deinit)
			vdev->deinit(vdev);
	}

	do_hooks((void *)vm, NULL, OS_HOOK_DESTROY_VM);

	if (vm->vcpus) {
		for (i = 0; i < vm->vcpu_nr; i++) {
			vcpu = vm->vcpus[i];
			if (!vcpu)
				continue;
			release_vcpu(vcpu);
		}

		free(vm->vcpus);
	}

	release_vm_memory(vm);

	i = vm->vmid;
	spin_lock_irqsave(&vms_lock, flags);
	clear_bit(i, vmid_bitmap);
	list_del(&vm->vm_list);
	vms[i] = NULL;
	total_vms--;
	spin_unlock_irqrestore(&vms_lock, flags);

	free(vm);
}

int vm_vcpus_init(struct vm *vm)
{
	struct vcpu *vcpu;

	vm_for_each_vcpu(vm, vcpu) {
		pr_notice("vm-%d vcpu-%d affnity to pcpu-%d\n",
				vm->vmid, vcpu->vcpu_id, vcpu_affinity(vcpu));

		/*
		 * init the vcpu context here.
		 */
		vcpu_vmodules_init(vcpu);

		if (!vm_is_native(vm)) {
			vcpu->vmcs->host_index = 0;
			vcpu->vmcs->guest_index = 0;
		}
	}

	/* some task will excuted after this function */
	vm_for_each_vcpu(vm, vcpu) {
		do_hooks(vcpu, NULL, OS_HOOK_VCPU_INIT);
		os_vcpu_power_on(vcpu, (unsigned long)vm->entry_point);
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
			pr_err("create vcpu:%d for %s failed\n", i, vm->name);
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

static void vm_open_ramdisk_file(struct vm *vm, struct vmtag *vme)
{
	if (!vm_is_native(vm))
		return;

	if (vme->kernel_file) {
		vm->kernel_file = malloc(sizeof(struct ramdisk_file));
		ASSERT(vm->kernel_file != NULL);
		ASSERT(ramdisk_open(vme->kernel_file, vm->kernel_file) == 0);
	}

	if (vme->dtb_file) {
		vm->dtb_file = malloc(sizeof(struct ramdisk_file));
		ASSERT(vm->dtb_file != NULL);
		ASSERT(ramdisk_open(vme->dtb_file, vm->dtb_file) == 0);
	}
		
	if (vme->initrd_file) {
		vm->initrd_file = malloc(sizeof(struct ramdisk_file));
		ASSERT(vm->initrd_file != NULL);
		ASSERT(ramdisk_open(vme->initrd_file, vm->initrd_file) == 0);
	}
}

static struct vm *__create_vm(struct vmtag *vme)
{
	struct vm *vm;

	if (vm_check_vcpu_affinity(vme->vmid, vme->vcpu_affinity,
				vme->nr_vcpu)) {
		pr_err("vcpu affinity for vm not correct\n");
		return NULL;
	}

	vm = malloc(sizeof(*vm));
	if (!vm)
		return NULL;

	vme->nr_vcpu = MIN(vme->nr_vcpu, VM_MAX_VCPU);
	memset(vm, 0, sizeof(struct vm));
	vm->vcpus = malloc(sizeof(struct vcpu *) * vme->nr_vcpu);
	if (!vm->vcpus) {
		free(vm);
		return NULL;
	}

	vm->vmid = vme->vmid;
	vm->flags |= vme->flags;
	strncpy(vm->name, vme->name, sizeof(vm->name) - 1);
	vm->vcpu_nr = vme->nr_vcpu;
	vm->entry_point = (void *)vme->entry;
	vm->setup_data = (void *)vme->setup_data;
	vm->load_address =
		(void *)(vme->load_address ? vme->load_address : vme->entry);
	vm->state = VM_STATE_OFFLINE;
	init_list(&vm->vdev_list);
	memcpy(vm->vcpu_affinity, vme->vcpu_affinity,
			sizeof(uint32_t) * VM_MAX_VCPU);

	/*
	 * open the ramdisk file if the vm need load from
	 * the ramdisk.
	 */
	vm_open_ramdisk_file(vm, vme);

	vms[vme->vmid] = vm;
	spin_lock(&vms_lock);
	list_add_tail(&vm_list, &vm->vm_list);
	total_vms++;
	spin_unlock(&vms_lock);

	vm->os = get_vm_os((char *)vme->os_type);

	return vm;
}

struct vm *create_vm(struct vmtag *vme)
{
	int ret = 0;
	struct vm *vm;

	if (vme->vmid != 0)  {
		pr_notice("request vmid %d\n", vme->vmid);
		if (test_and_set_bit(vme->vmid, vmid_bitmap))
			return NULL;
	} else {
		vme->vmid = alloc_new_vmid();
		if (vme->vmid == 0)
			return NULL;
	}

	vm = __create_vm(vme);
	if (!vm)
		return NULL;

	ret = vm_mm_struct_init(vm);
	if (ret) {
		pr_err("mm struct init failed\n");
		goto release_vm;
	}

	iommu_vm_init(vm);

	ret = create_vcpus(vm);
	if (ret) {
		pr_err("create vcpus for vm failded\n");
		ret = 0;
		goto release_vm;
	}

	if (do_hooks((void *)vm, NULL, OS_HOOK_CREATE_VM)) {
		pr_err("create vm failed in hook function\n");
		goto release_vm;
	}

	if ((vm->flags & VM_FLAGS_HOST)) {
		ASSERT(host_vm == NULL);
		host_vm = vm;
	}

	return vm;

release_vm:
	destroy_vm(vm);

	return NULL;
}

struct vm *get_host_vm(void)
{
	return host_vm;
}

static inline const char *get_vm_type(struct vm *vm)
{
	if (vm->flags & VM_FLAGS_HOST)
		return "Host";
	else if (vm->flags & VM_FLAGS_NATIVE)
		return "Native";
	else
		return "Guest";
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

	pr_notice("**** create new vm ****\n");
	pr_notice("    vmid: %d\n", vmtag.vmid);
	pr_notice("    name: %s\n", vmtag.name);
	pr_notice("    os_type: %s\n", vmtag.os_type);
	pr_notice("    nr_vcpu: %d\n", vmtag.nr_vcpu);
	pr_notice("    entry: 0x%p\n", vmtag.entry);
	pr_notice("    setup_data: 0x%p\n", vmtag.setup_data);
	pr_notice("    load-address: 0x%p\n", vmtag.load_address);
	pr_notice("    kernel-file: %s\n", vmtag.kernel_file ? vmtag.kernel_file : "NULL");
	pr_notice("    dtb-file: %s\n", vmtag.dtb_file ? vmtag.dtb_file : "NULL");
	pr_notice("    initrd-file: %s\n", vmtag.initrd_file ? vmtag.initrd_file : "NULL");
	pr_notice("    %s-bit vm\n", vmtag.flags & VM_FLAGS_32BIT ? "32" : "64");
	pr_notice("    flags: 0x%x\n", vmtag.flags);
	pr_notice("    affinity: %d %d %d %d %d %d %d %d\n",
			vmtag.vcpu_affinity[0], vmtag.vcpu_affinity[1],
			vmtag.vcpu_affinity[2], vmtag.vcpu_affinity[3],
			vmtag.vcpu_affinity[4], vmtag.vcpu_affinity[5],
			vmtag.vcpu_affinity[6], vmtag.vcpu_affinity[7]);

	vm = create_vm(&vmtag);
	if (!vm) {
		pr_err("create vm-%d failed\n", vmtag.vmid);
		return NULL;
	}

	vm->dev_node = node;

	/* parse the memory information of the vm from dtb */
	ret = of_get_u64_array(node, "memory", meminfo, 2 * VM_MAX_MEM_REGIONS);
	if ((ret <= 0) || ((ret % 2) != 0)) {
		pr_err("get wrong memory information for vm-%d", vmtag.vmid);
		destroy_vm(vm);

		return NULL;
	}

	ret = ret / 2;

	for (i = 0; i < ret; i ++) {
		pr_notice("VM%d MEM [0x%x 0x%x]\n", vm->vmid,
				meminfo[i * 2], meminfo[i * 2 + 1]);
		split_vmm_area(&vm->mm, meminfo[i * 2],
				meminfo[i * 2 + 1], VM_NATIVE_NORMAL);
	}

	pr_notice("create vm%d [%s %s] done\n", vm->vmid,
			vm->name, get_vm_type(vm));

	return vm;
}

static void parse_and_create_vms(void)
{
#ifdef CONFIG_DEVICE_TREE
	struct device_node *node;

	node = of_find_node_by_name(of_root_node, "vms");
	if (node)
		of_iterate_all_node_loop(node, create_native_vm_of, NULL);
#endif
}

static int of_create_vmboxs(void)
{
	struct device_node *mailboxes;
	struct device_node *child;

	mailboxes = of_find_node_by_name(of_root_node, "vmboxs");
	if (!mailboxes)
		return -ENOENT;

	/* parse each mailbox entry and create it */
	of_node_for_each_child(mailboxes, child) {
		if (of_create_vmbox(child))
			pr_err("create vmbox [%s] fail\n", child->name);
		else
			pr_notice("create vmbox [%s] successful\n", child->name);
	}

	return 0;
}

int virt_init(void)
{
	extern void vmm_init(void);
	extern void vm_daemon_init(void);
	struct vm *vm;

	/*
	 * VMID 0 is reserved
	 */
	set_bit(0, vmid_bitmap);
	vmm_init();

	vm_daemon_init();

	parse_and_create_vms();

	/* check whether host VM has been create correctly */
	vm = get_host_vm();
	if (!vm) {
		pr_err("hvm has not been create correctly\n");
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
		os_vm_init(vm);
		setup_native_vm(vm);
		vm_mm_init(vm);
		load_vm_image(vm);
		vm_vcpus_init(vm);
	}

	return 0;
}

void start_all_vm(void)
{
	struct vm *vm;

	list_for_each_entry(vm, &vm_list, vm_list)
		start_vm(vm);
}

/*
 * vm start 0 - start the vm which vmid is 0
 */
static int vm_command_hdl(int argc, char **argv)
{
	uint32_t vmid;

	if (argc > 2 && strcmp(argv[1], "start") == 0) {
		vmid = atoi(argv[2]);
		if (vmid == 0)
			start_all_vm();
		else
			start_vm(get_vm_by_id(vmid));
	}

	return 0;
}
DEFINE_SHELL_COMMAND(vm, "vm", "virtual machine cmd",
		vm_command_hdl, 2);
