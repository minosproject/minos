/*
 * Copyright (C) 2019 Min Le (lemin9538@gmail.com)
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
#include <common/hypervisor.h>

static int fdt_setup_other(struct vm *vm)
{
	int node;
	void *dtb = vm->setup_data;

	/* delete the vms node which no longer use */
	node = fdt_path_offset(dtb, "/vms");
	if (node)
		fdt_del_node(dtb, node);

	return 0;
}

static int fdt_setup_minos(struct vm *vm)
{
	int node, i;
	uint32_t *tmp = NULL;
	size_t size;
	int vspi_nr = vm->vspi_nr;
	struct virq_chip *vc = vm->virq_chip;
	void *dtb = vm->setup_data;

	node = fdt_path_offset(dtb, "/minos");
	if (node < 0) {
		node = fdt_add_subnode(dtb, 0, "minos");
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

	fdt_setprop(dtb, node, "compatible", "minos,hypervisor", 17);

	size = 0;
	if (vc && vc->vm0_virq_data) {
		size = vc->vm0_virq_data(tmp, vspi_nr,
				vm->flags & VM_FLAGS_SETUP_MASK);
	}

	if (size) {
		i = fdt_setprop(dtb, node, "interrupts", (void *)tmp, size);
		if (i)
			pr_err("fdt set interrupt for minos failed\n");
	}

	free(tmp);
	return i;
}

static int fdt_setup_cmdline(struct vm *vm)
{
	int node, len, chosen_node;
	char *new_cmdline;
	char buf[512];
	void *dtb = vm->setup_data;
	extern void *hv_dtb;

	chosen_node = fdt_path_offset(dtb, "/chosen");
	if (chosen_node < 0) {
		chosen_node = fdt_add_subnode(dtb, 0, "chosen");
		if (chosen_node < 0) {
			pr_err("add chosen node failed for vm0\n");
			return chosen_node;
		}
	}

	node = fdt_path_offset(hv_dtb, "/vms/vm0");
	if (node < 0)
		return 0;

	new_cmdline = (char *)fdt_getprop(hv_dtb, node, "cmdline", &len);
	if (!new_cmdline || len <= 0) {
		pr_info("no new cmdline using default\n");
		return 0;
	}

	if (len >= 512)
		pr_warn("new cmdline is too big %d\n", len);

	/*
	 * can not directly using new_cmdline in fdt_setprop
	 * do not know why, there may a issue in libfdt or
	 * other reason
	 */
	buf[511] = 0;
	strncpy(buf, new_cmdline, MIN(511, len));
	fdt_setprop(dtb, chosen_node, "bootargs", buf, len);

	return 0;
}

static int fdt_setup_cpu(struct vm *vm)
{
	int offset, node, i;
	char name[16];
	void *dtb = vm->setup_data;

	/*
	 * delete unused vcpu for hvm
	 */
	offset = of_get_node_by_name(dtb, 0, "cpus");
	if (offset < 0) {
		pr_err("can not find cpus node in dtb\n");
		return -ENOENT;
	}

	node = fdt_subnode_offset(dtb, offset, "cpu-map");
	if (node > 0) {
		pr_info("delete cpu-map node\n");
		fdt_del_node(dtb, node);
	}

	memset(name, 0, 16);
	for (i = vm->vcpu_nr; i < CONFIG_MAX_CPU_NR; i++) {
		sprintf(name, "cpu@%x", ((i / 4) << 8) + (i % 4));
		node = fdt_subnode_offset(dtb, offset, name);
		if (node >= 0) {
			pr_info("delete vcpu %s for vm0\n", name);
			fdt_del_node(dtb, node);
		}
	}

	return 0;
}

static int fdt_setup_memory(struct vm *vm)
{
	int offset, size, i;
	int size_cell, address_cell;
	uint32_t *args, *tmp;
	unsigned long mstart, msize;
	void *dtb = vm->setup_data;

	offset = of_get_node_by_name(dtb, 0, "memory");
	if (offset < 0) {
		offset = fdt_add_subnode(dtb, 0, "memory");
		if (offset < 0)
			return offset;

		fdt_setprop(dtb, offset, "device_type", "memory", 7);
	}

	size_cell = fdt_n_size_cells(dtb, offset);
	address_cell = fdt_n_addr_cells(dtb, offset);
	pr_info("%s size-cells:%d address-cells:%d\n",
			__func__, size_cell, address_cell);

	if ((size_cell < 1) || (address_cell < 1))
		return -EINVAL;

	tmp = args = (uint32_t *)get_free_page();
	if (!args)
		return -ENOMEM;

	size = 0;

	for (i = 0; i < vm->mm.nr_mem_regions; i++) {
		mstart = vm->mm.memory_regions[i].phy_base;
		msize = vm->mm.memory_regions[i].size;

		pr_info("add memory region to vm0 0x%p 0x%p\n", mstart, msize);

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

void fdt_vm_init(struct vm *vm)
{
	void *fdt = vm->setup_data;

	create_host_mapping((vir_addr_t)fdt, (phy_addr_t)fdt,
			MEM_BLOCK_SIZE, 0);

	fdt_open_into(fdt, fdt, MAX_DTB_SIZE);
	if(fdt_check_header(fdt)) {
		pr_err("invaild dtb after open into\n");
		return;
	}

	if (vm_is_hvm(vm))
		fdt_setup_minos(vm);

	fdt_setup_cmdline(vm);
	fdt_setup_cpu(vm);
	fdt_setup_memory(vm);
	fdt_setup_other(vm);

	if (platform->setup_hvm && vm_is_hvm(vm))
		platform->setup_hvm(vm, fdt);

	fdt_pack(fdt);
	flush_dcache_range((unsigned long)fdt, MAX_DTB_SIZE);
	destroy_host_mapping((unsigned long)fdt, MAX_DTB_SIZE);
}
