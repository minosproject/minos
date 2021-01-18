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
#include <libfdt/libfdt.h>
#include <minos/platform.h>
#include <minos/of.h>
#include <config/config.h>

void *hv_dtb = NULL;
struct device_node *hv_node;

int fdt_spin_table_init(phy_addr_t *smp_holding)
{
	int offset, node, i, len;
	char name[16];
	const void *data;
	fdt32_t *tmp;

	/*
	 * if the smp cpu boot using spin table
	 * get the spin table address and these
	 * address must mapped to the hypervisor
	 * address space
	 */

	offset = of_get_node_by_name(hv_dtb, 0, "cpus");
	if (offset <= 0) {
		pr_err("can not find cpus node in dtb\n");
		return -EINVAL;
	}

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		sprintf(name, "cpu@%x", cpuid_to_affinity(i));
		node = of_get_node_by_name(hv_dtb, offset, name);
		if (node <= 0) {
			pr_err("can not find %s\n", name);
			continue;
		}

		/* get the enable methold content */
		data = fdt_getprop(hv_dtb, node, "enable-method", &len);
		if (!data || len <= 0)
			continue;

		if (strncmp("spin-table", (char *)data, 10))
			continue;

		/* read the holding address */
		data = fdt_getprop(hv_dtb, node, "cpu-release-addr", &len);
		if (!data || len <= sizeof(uint32_t))
			continue;

		len = len / sizeof(uint32_t);
		tmp = (uint32_t *)data;
		if (len == 1)
			smp_holding[i] = fdt32_to_cpu(tmp[0]);
		else
			smp_holding[i] = fdt32_to_cpu64(tmp[0], tmp[1]);

		pr_notice("%s using spin-table relase addr 0x%p\n",
				name, smp_holding[i]);
	}

	return 0;
}

static void fdt_setup_platform(void)
{
	int len;
	const void *data;

	data = fdt_getprop(hv_dtb, 0, "model", &len);
	if (data)
		pr_notice("model : %s\n", (char *)data);

	/*
	 * compatible may like arm,fvp-base\0arm,vexpress\0
	 * but here, only parsing the first string
	 */
	data = fdt_getprop(hv_dtb, 0, "compatible", &len);
	if (data) {
		pr_notice("platform : %s\n", (char *)data);
		platform_set_to((const char *)data);
	}
}

int fdt_init(void)
{
	if (!hv_dtb)
		panic("dtb address is not set\n");

	hv_node = of_parse_device_tree(hv_dtb);
	if (!hv_node)
		pr_warn("root device node create failed\n");

	return 0;
}

static int __fdt_parse_memory_info(int node, char *attr)
{
	int len = 0;
	uint64_t base, size;
	int size_cell, address_cell;
	fdt32_t *v;

	v = (fdt32_t *)fdt_getprop(hv_dtb, node, attr, &len);
	if (!v || (len < 8)) {
		pr_warn("no memory info found in dtb\n");
		return -ENOENT;
	}

	len = len / 4;
	size_cell = fdt_n_size_cells(hv_dtb, node);
	address_cell = fdt_n_addr_cells(hv_dtb, node);
	pr_notice("memory node address_cells:%d size_cells:%d\n",
			address_cell, size_cell);
	if ((size_cell > 2) || (size_cell == 0)) {
		pr_warn("memory node size_cells not correct\n");
		return -EINVAL;
	}

	if ((address_cell > 2) || (address_cell == 0)) {
		pr_warn("memory node address_cells not correct\n");
		return -EINVAL;
	}

	while (len >= (size_cell + address_cell)) {
		if (address_cell == 2) {
			base = fdt32_to_cpu64(v[0], v[1]);
			v += 2;
		} else {
			base = fdt32_to_cpu(v[0]);
			v++;
		}

		if (size_cell == 2) {
			size = fdt32_to_cpu64(v[0], v[1]);
			v += 2;
		} else {
			size = fdt32_to_cpu(v[0]);
			v += 1;
		}

		len -= size_cell + address_cell;
		add_memory_region(base, size, MEMORY_REGION_F_NORMAL);
	}

	return 0;
}

static void __fdt_parse_dtb_mem(void)
{
	size_t size;

	/*
	 * reserve the dtb memory, the dtb memeory size keep
	 * 4K align
	 */
	size = fdt_totalsize(hv_dtb);
	pr_notice("DTB - 0x%x ---> 0x%x\n", (unsigned long)hv_dtb, size);
	size = BALIGN(size, PAGE_SIZE);
	split_memory_region((phy_addr_t)hv_dtb, size, MEMORY_REGION_F_DTB);
}

static void __fdt_parse_memreserve(void)
{
	int count, i, ret;
	uint64_t address, size;

	count = fdt_num_mem_rsv(hv_dtb);
	if (count == 0)
		return;

	for (i = 0; i < count; i++) {
		ret = fdt_get_mem_rsv(hv_dtb, i, &address, &size);
		if (ret)
			continue;

		pr_notice("find rev memory - id: %d addr: 0x%x size: 0x%x\n",
				i, address, size);
		split_memory_region(address, size, MEMORY_REGION_F_RSV);
	}
}

#ifdef CONFIG_VIRT
static void __fdt_parse_vm_mem(void)
{
	const char *name;
	const char *type;
	int node, child, len, i;
	uint64_t array[2 * 10];
	phy_addr_t base;
	size_t size;

	node = fdt_path_offset(hv_dtb, "/vms");
	if (node <= 0) {
		pr_warn("no virtual machine found in dts\n");
		return;
	}

	fdt_for_each_subnode(child, hv_dtb, node) {
		type = (char *)fdt_getprop(hv_dtb, child, "device_type", &len);
		if (!type)
			continue;
		if (strcmp(type, "virtual_machine") != 0)
			continue;

		/*
		 * get the memory information for the vm, each vm will
		 * have max 10 memory region
		 */
		len = __of_get_u64_array(hv_dtb, child, "memory", array, 2 * 10);
		if ((len <= 0) || ((len % 2) != 0)) {
			name = fdt_get_name(hv_dtb, child, NULL);
			pr_err("wrong memory information for %s\n",
					name ? name : "unknown");
			continue;
		}

		for (i = 0; i < len; i += 2 ) {
			base = (phy_addr_t)array[i];
			size = (size_t)array[i + 1];
			split_memory_region(base, size, MEMORY_REGION_F_VM);
		}
	}
}
#endif

static void __fdt_parse_kernel_mem(void)
{
	split_memory_region(CONFIG_MINOS_START_ADDRESS,
			CONFIG_MINOS_RAM_SIZE, MEMORY_REGION_F_KERNEL);
}

static void __fdt_parse_ramdisk_mem(void)
{
	extern void set_ramdisk_address(void *start, void *end);
	const fdt64_t *data;
	uint64_t start, end;
	int node, len;

	node = fdt_path_offset(hv_dtb, "/chosen");
	if (node <= 0)
		return;

	data = fdt_getprop(hv_dtb, node, "minos,initrd-start", &len);
	if (!data || (len == 0))
		return;
	start = fdt64_to_cpu(*data);

	data = fdt_getprop(hv_dtb, node, "minos,initrd-end", &len);
	if (!data || (len == 0))
		return;
	end = fdt64_to_cpu(*data);

	set_ramdisk_address((void *)start, (void *)end);
	split_memory_region(start, PAGE_BALIGN(end - start),
			MEMORY_REGION_F_RAMDISK);
}

int fdt_parse_memory_info(void)
{
	int node;

	node = of_get_node_by_name(hv_dtb, 0, "memory");
	if (node <= 0) {
		pr_warn("no memory node found in dtb\n");
		return -ENOENT;
	}

	__fdt_parse_memory_info(node, "reg");

	/*
	 * split the minos kernel's memory region, need
	 * before to split the dtb memory
	 */
	__fdt_parse_kernel_mem();
	__fdt_parse_memreserve();
	__fdt_parse_dtb_mem();
	__fdt_parse_ramdisk_mem();

#ifdef CONFIG_VIRT
	__fdt_parse_vm_mem();
#endif

	return 0;
}

int fdt_early_init(void)
{
	pr_notice("using device tree @0x%x\n", hv_dtb);

	/*
	 * set up the platform from the dtb file then get the spin
	 * table information if the platform is using spin table to
	 * wake up other cores
	 */
	fdt_setup_platform();

	return 0;
}

