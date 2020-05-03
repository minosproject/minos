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
#include <minos/mm.h>
#include <virt/vmm.h>
#include <libfdt/libfdt.h>
#include <virt/vm.h>
#include <minos/platform.h>
#include <minos/of.h>
#include <config/config.h>
#include <virt/virq_chip.h>
#include <virt/virq.h>
#include <virt/vmbox.h>
#include <minos/sched.h>
#include <minos/arch.h>
#include <virt/os.h>
#include <virt/resource.h>
#include <common/hypervisor.h>

static int fdt_setup_cmdline(struct vm *vm)
{
	int node, len, chosen_node;
	char *new_cmdline;
	char buf[128];
	void *dtb = vm->setup_data;
	extern void *hv_dtb;

	chosen_node = fdt_path_offset(dtb, "/chosen");
	if (chosen_node < 0) {
		chosen_node = fdt_add_subnode(dtb, 0, "chosen");
		if (chosen_node < 0) {
			pr_err("add chosen node failed for vm%d\n", vm->vmid);
			return chosen_node;
		}
	}

	len = sprintf(buf, "/vms/vm%d", vm->vmid);
	buf[len] = 0;

	node = fdt_path_offset(hv_dtb, buf);
	if (node < 0)
		return 0;

	new_cmdline = (char *)fdt_getprop(hv_dtb, node, "cmdline", &len);
	if (!new_cmdline || len <= 0) {
		pr_notice("no new cmdline using default\n");
		return 0;
	}

	pr_notice("New cmdline: %s\n", new_cmdline);

	/*
	 * can not directly using new_cmdline in fdt_setprop
	 * do not know why, there may a issue in libfdt or
	 * other reason
	 */
	fdt_setprop(dtb, chosen_node, "bootargs", new_cmdline, len);

	return 0;
}

static int fdt_setup_cpu(struct vm *vm)
{
	int offset, node, i;
	char name[16];
	void *dtb = vm->setup_data;
	uint64_t aff_id;

	/*
	 * delete unused vcpu for hvm
	 */
	offset = of_get_node_by_name(dtb, 0, "cpus");
	if (offset < 0) {
		pr_err("can not find cpus node in dtb\n");
		return -ENOENT;
	}

	for (i = vm->vcpu_nr; i < CONFIG_MAX_CPU_NR; i++) {
		if (vm_is_hvm(vm))
			aff_id = cpuid_to_affinity(i);
		else
			aff_id = i;

		sprintf(name, "cpu@%x", aff_id);
		node = fdt_subnode_offset(dtb, offset, name);
		if (node >= 0) {
			pr_notice("delete vcpu %s for vm%d\n", name, vm->vmid);
			fdt_del_node(dtb, node);
		}
	}

	return 0;
}

static int fdt_setup_memory(struct vm *vm)
{
	int offset, size;
	int size_cell, address_cell;
	uint32_t *args, *tmp;
	unsigned long mstart, msize;
	void *dtb = vm->setup_data;
	struct vmm_area *va;

	offset = of_get_node_by_name(dtb, 0, "memory");
	if (offset < 0) {
		offset = fdt_add_subnode(dtb, 0, "memory");
		if (offset < 0)
			return offset;

		fdt_setprop(dtb, offset, "device_type", "memory", 7);
	}

	size_cell = fdt_n_size_cells(dtb, offset);
	address_cell = fdt_n_addr_cells(dtb, offset);
	pr_notice("%s size-cells:%d address-cells:%d\n",
			__func__, size_cell, address_cell);

	if ((size_cell < 1) || (address_cell < 1))
		return -EINVAL;

	tmp = args = (uint32_t *)get_free_page();
	if (!args)
		return -ENOMEM;

	size = 0;

	list_for_each_entry(va, &vm->mm.vmm_area_used, list) {
		if (!(va->flags & VM_NORMAL))
			continue;

		mstart = va->start;
		msize = va->size;

		pr_notice("add memory region to vm%d 0x%p 0x%p\n",
				vm->vmid, mstart, msize);

		if (address_cell == 1) {
			*args++ = cpu_to_fdt32(mstart);
			size++;
		} else {
			*args++ = cpu_to_fdt32(mstart >> 32);
			*args++ = cpu_to_fdt32(mstart);
			size += 2;
		}

		if (size_cell ==  1) {
			*args++ = cpu_to_fdt32(msize);
			size++;
		} else {
			*args++ = cpu_to_fdt32(msize >> 32);
			*args++ = cpu_to_fdt32(msize);
			size += 2;
		}
	}

	fdt_setprop(dtb, offset, "reg", (void *)tmp, size * 4);
	free(args);

	return 0;
}

static void fdt_vm_init(struct vm *vm)
{
	void *fdt = vm->setup_data;

	fdt_open_into(fdt, fdt, MAX_DTB_SIZE);
	if(fdt_check_header(fdt)) {
		pr_err("invaild dtb after open into\n");
		return;
	}

	fdt_setup_cmdline(vm);
	fdt_setup_cpu(vm);
	fdt_setup_memory(vm);

	fdt_pack(fdt);
	flush_dcache_range((unsigned long)fdt, MAX_DTB_SIZE);
}

static void lushan_vcpu_init(struct vcpu *vcpu)
{
	gp_regs *regs;

	/* fill the dtb address to x0 */
	if (get_vcpu_id(vcpu) == 0) {
		arch_init_vcpu(vcpu, (void *)vcpu->vm->entry_point, NULL);
		regs = (gp_regs *)vcpu->task->stack_base;

		if (task_is_64bit(vcpu->task))
			regs->x0 = (uint64_t)vcpu->vm->setup_data;
		else {
			regs->x0 = 0;
			regs->x1 = 2272;		/* arm vexpress machine type */
			regs->x2 = (uint64_t)vcpu->vm->setup_data;
		}
	}
}

static void lushan_vcpu_power_on(struct vcpu *vcpu, unsigned long entry)
{
	gp_regs *regs;

	arch_init_vcpu(vcpu, (void *)entry, NULL);
	regs = (gp_regs *)vcpu->task->stack_base;

	regs->elr_elx = entry;
	regs->x0 = 0;
	regs->x1 = 0;
	regs->x2 = 0;
	regs->x3 = 0;
}

static void lushan_vm_setup(struct vm *vm)
{
	fdt_vm_init(vm);
}

static int lushan_create_native_vm_resource(struct vm *vm)
{
	if (vm->setup_data) {
		if (of_data(vm->setup_data)) {
			vm->flags |= VM_FLAGS_SETUP_OF;
			create_vm_resource_of(vm, vm->setup_data);
		}
	}

	/*
	 * check whether there are some resource need
	 * to created from the hypervisor's dts
	 */
	create_native_vm_resource_common(vm);

	return 0;
}

static int lushan_create_guest_vm_resource(struct vm *vm)
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

struct os_ops lushan_os_ops = {
	.vcpu_init 	= lushan_vcpu_init,
	.vcpu_power_on 	= lushan_vcpu_power_on,
	.vm_setup 	= lushan_vm_setup,
	.create_nvm_res = lushan_create_native_vm_resource,
	.create_gvm_res = lushan_create_guest_vm_resource,
};

static int __init_text os_lushan_init(void)
{
	return register_os("lushan", OS_TYPE_LUSHAN, &lushan_os_ops);
}
module_initcall(os_lushan_init);
