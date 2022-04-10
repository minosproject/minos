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
#include <generic/hypervisor.h>
#include <minos/ramdisk.h>
#include <asm/cache.h>

static int fdt_setup_other(struct vm *vm)
{
	return 0;
}

static int fdt_setup_romflash(struct vm *vm)
{
	void *dtb = (void *)ptov(vm->setup_data);
	unsigned long addr;
	struct vmm_area *va;
	uint32_t size;
	char name[128];
	int node;

	if (!vm->initrd_file)
		return 0;

	/*
	 * find the best memory region for initrd ramdisk.
	 */
	size = ramdisk_file_size(vm->initrd_file);
	va = alloc_free_vmm_area(&vm->mm, size, PAGE_MASK,
			VM_IO | __VM_PFNMAP | VM_MAP_PT | VM_RW);
	if (!va)
		return -ENOMEM;

	addr = vtop(ramdisk_file_base(vm->initrd_file));
	if (map_vmm_area(&vm->mm, va, addr)) {
		pr_err("map ramdisk memory to vm%d failed\n", vm->vmid);
		release_vmm_area(&vm->mm, va);
		return -ENOMEM;
	}

	/*
	 * add initrd information to the device tree. Here the
	 * memory region will used as a map_rom in linux system, so
	 * add a rom device to the dts.
	 */
	memset(name, 0, 128);
	strcpy(name, "of-flash");
	node = fdt_add_subnode(dtb, 0, name);
	if (node < 0) {
		pr_err("failed to add rom flash device to vm%d\n", vm->vmid);
		unmap_vmm_area(&vm->mm, va);
		release_vmm_area(&vm->mm, va);
	}

	fdt_setprop(dtb, node, "compatible", "minos,rom-flash", 17);
	fdt_set_node_reg(dtb, node, va->start, PAGE_BALIGN(size));
	fdt_setprop(dtb, node, "linux,mtd-name", "rom", 4);

	return 0;
}

static int fdt_setup_minos(struct vm *vm)
{
	void *dtb = (void *)ptov(vm->setup_data);
	int node;

	node = fdt_path_offset(dtb, "/minos");
	if (node < 0) {
		node = fdt_add_subnode(dtb, 0, "minos");
		if (node < 0)
			return node;
	}

	fdt_setprop(dtb, node, "compatible", "minos,hypervisor", 17);
	return 0;
}

static int fdt_setup_vm_virqs(struct vm *vm)
{
	int node, i;
	uint32_t *tmp = NULL;
	size_t size;
	int vspi_nr = vm->vspi_nr;
	struct virq_chip *vc = vm->virq_chip;
	void *dtb = (void *)ptov(vm->setup_data);

	node = fdt_path_offset(dtb, "/vm_fake_device");
	if (node < 0) {
		node = fdt_add_subnode(dtb, 0, "vm_fake_device");
		if (node < 0)
			return node;
	}

	size = vspi_nr * sizeof(uint32_t) * 3;
	size = PAGE_BALIGN(size);
	tmp = (uint32_t *)get_free_pages(PAGE_NR(size));
	if (!tmp) {
		pr_err("fdt setup minos failed no memory\n");
		return -ENOMEM;
	}

	fdt_setprop(dtb, node, "compatible", "minos,fakedev", 17);
	size = 0;

	if (vc && vc->generate_virq) {
		for (i = 0; i < vspi_nr; i++) {
			if (!virq_need_export(vm->vcpus[0],
					i + VM_LOCAL_VIRQ_NR))
				continue;
			size += vc->generate_virq(tmp + size,
					i + VM_LOCAL_VIRQ_NR);
		}
	}

	if (size) {
		i = fdt_setprop(dtb, node, "interrupts",
				(void *)tmp, size * sizeof(uint32_t));
		if (i)
			pr_err("fdt set interrupt for minos failed\n");
	}

	free_pages(tmp);

	return i;
}

static int fdt_setup_cmdline(struct vm *vm)
{
	int node, len, chosen_node;
	char *new_cmdline;
	char buf[128];
	void *vm_dtb = (void *)ptov(vm->setup_data);
	void *hv_dtb = dtb_address;

	chosen_node = fdt_path_offset(vm_dtb, "/chosen");
	if (chosen_node < 0) {
		chosen_node = fdt_add_subnode(vm_dtb, 0, "chosen");
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
	fdt_setprop(vm_dtb, chosen_node, "bootargs", new_cmdline, len);

	return 0;
}

static int fdt_setup_cpu(struct vm *vm)
{
	int offset, node, i;
	char name[16];
	void *dtb = (void *)ptov(vm->setup_data);
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
		if (vm_is_host_vm(vm))
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
	void *dtb = (void *)ptov(vm->setup_data);
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
	free_pages(tmp);

	return 0;
}

static void fdt_vm_init(struct vm *vm)
{
	void *fdt = (void *)ptov(vm->setup_data);

	fdt_open_into(fdt, fdt, MAX_DTB_SIZE);
	if(fdt_check_header(fdt)) {
		pr_err("invaild dtb after open into\n");
		return;
	}

	if (vm_is_host_vm(vm))
		fdt_setup_minos(vm);

	/*
	 * current need to export all the irq number
	 * to the VM, if one device need to request
	 * the virq dynmaic, TO BE FIXED
	 */
	if (vm_is_native(vm))
		fdt_setup_vm_virqs(vm);

	fdt_setup_cmdline(vm);
	fdt_setup_cpu(vm);
	fdt_setup_memory(vm);
	fdt_setup_romflash(vm);
	fdt_setup_other(vm);

	vmbox_init(vm);

	if (platform->setup_hvm && vm_is_host_vm(vm))
		platform->setup_hvm(vm, fdt);

	fdt_pack(fdt);
}

static inline void __linux_vcpu_startup(struct vcpu *vcpu, unsigned long entry)
{
	gp_regs *regs;

	arch_vcpu_init(vcpu, (void *)entry, NULL);
	regs = (gp_regs *)vcpu->task->stack_base;

	/* fill the dtb address to x0 */
	if (get_vcpu_id(vcpu) == 0) {
		if (!task_is_32bit(vcpu->task)) {
			regs->x0 = (uint64_t)vcpu->vm->setup_data;
		} else {
			regs->x0 = 0;
			regs->x1 = 2272;		/* arm vexpress machine type */
			regs->x2 = (uint64_t)vcpu->vm->setup_data;
		}
	} else {
		regs->x0 = 0;
		regs->x1 = 0;
		regs->x2 = 0;
		regs->x3 = 0;
	}
}

static void linux_vcpu_startup(struct vcpu *vcpu)
{
	__linux_vcpu_startup(vcpu, (unsigned long)vcpu->vm->entry_point);
}

static void linux_vcpu_power_on(struct vcpu *vcpu, unsigned long entry)
{
	__linux_vcpu_startup(vcpu, entry);
}

static void linux_vm_setup(struct vm *vm)
{
	fdt_vm_init(vm);
}

static int linux_create_native_vm_resource(struct vm *vm)
{
	if (vm->flags & VM_FLAGS_NATIVE) {
		if (vm->kernel_file && vm->dtb_file)
			vm->flags |= VM_FLAGS_CAN_RESET;
	}

	if (vm->setup_data) {
		if (of_data((void *)ptov(vm->setup_data))) {
			vm->flags |= VM_FLAGS_SETUP_OF;
			create_vm_resource_of(vm, (void *)ptov(vm->setup_data));
		}
	}

	/*
	 * check whether there are some resource need
	 * to created from the hypervisor's dts
	 */
	create_native_vm_resource_common(vm);

	return 0;
}

static int linux_create_guest_vm_resource(struct vm *vm)
{
	phy_addr_t addr;


	// fixme need 

	/*
	 * convert the guest's memory to hypervisor's memory space
	 * do not need to map again, since all the guest VM's memory
	 * has been mapped when mm_init()
	 */
	if (translate_guest_ipa(&vm->mm, (unsigned long)vm->setup_data, &addr));
	if (!addr)
		return -ENOMEM;

	return create_vm_resource_of(vm, (void *)addr);
}

struct os_ops linux_os_ops = {
	.vcpu_init 	= linux_vcpu_startup,
	.vcpu_power_on 	= linux_vcpu_power_on,
	.vm_setup 	= linux_vm_setup,
	.create_nvm_res = linux_create_native_vm_resource,
	.create_gvm_res = linux_create_guest_vm_resource,
};

static int __init_text os_linux_init(void)
{
	return register_os("linux", OS_TYPE_LINUX, &linux_os_ops);
}
module_initcall(os_linux_init);
