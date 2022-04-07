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
#include <libfdt/libfdt.h>
#include <minos/of.h>
#include <config/config.h>
#include <minos/memory.h>
#include <minos/ramdisk.h>

static int __fdt_parse_memory_info(int node, char *attr)
{
	int len = 0;
	uint64_t base, size;
	int size_cell, address_cell;
	fdt32_t *v;

	v = (fdt32_t *)fdt_getprop(dtb_address, node, attr, &len);
	if (!v || (len < 8)) {
		pr_warn("no memory info found in dtb\n");
		return -ENOENT;
	}

	len = len / 4;
	size_cell = fdt_n_size_cells(dtb_address, node);
	address_cell = fdt_n_addr_cells(dtb_address, node);
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
		add_memory_region(base, size, MEMORY_REGION_TYPE_NORMAL, 0);
	}

	return 0;
}

static void __fdt_parse_memreserve(void)
{
	int count, i, ret;
	uint64_t address, size;

	/*
	 * system can reseve some memory at the head of minos
	 * memory space
	 */
	if (CONFIG_MINOS_ENTRY_ADDRESS != minos_start) {
		size = CONFIG_MINOS_ENTRY_ADDRESS - minos_start;
		split_memory_region(minos_start, size, MEMORY_REGION_TYPE_RSV, 1);
	}

	count = fdt_num_mem_rsv(dtb_address);
	if (count == 0)
		return;

	for (i = 0; i < count; i++) {
		ret = fdt_get_mem_rsv(dtb_address, i, &address, &size);
		if (ret)
			continue;

		pr_notice("find rev memory - id: %d addr: 0x%x size: 0x%x\n",
				i, address, size);
		split_memory_region(address, size, MEMORY_REGION_TYPE_RSV, 0);
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
	int vmid;

	node = fdt_path_offset(dtb_address, "/vms");
	if (node <= 0) {
		pr_warn("no virtual machine found in dts\n");
		return;
	}

	fdt_for_each_subnode(child, dtb_address, node) {
		type = (char *)fdt_getprop(dtb_address, child, "device_type", &len);
		if (!type || (strcmp(type, "virtual_machine") != 0))
			continue;

		__of_get_u32_array(dtb_address, child, "vmid", (uint32_t *)&vmid, 1);

		/*
		 * get the memory information for the vm, each vm will
		 * have max 10 memory region
		 */
		len = __of_get_u64_array(dtb_address, child, "memory", array, 2 * 10);
		if ((len <= 0) || ((len % 2) != 0)) {
			name = fdt_get_name(dtb_address, child, NULL);
			pr_err("wrong memory information for %s\n",
					name ? name : "unknown");
			continue;
		}

		for (i = 0; i < len; i += 2 ) {
			base = (phy_addr_t)array[i];
			size = (size_t)array[i + 1];
			split_memory_region(base, size, MEMORY_REGION_TYPE_VM, vmid);
		}
	}
}
#endif

static void __fdt_parse_kernel_mem(void)
{
	split_memory_region(minos_start, CONFIG_MINOS_RAM_SIZE,
			MEMORY_REGION_TYPE_KERNEL, 0);
}

static void __fdt_parse_ramdisk_mem(void)
{
	const fdt32_t *data;
	uint64_t start, end;
	int node, len;

	node = fdt_path_offset(dtb_address, "/chosen");
	if (node <= 0)
		return;

	data = fdt_getprop(dtb_address, node, "minos,ramdisk-start", &len);
	if (!data || (len == 0))
		return;
	start = fdt32_to_cpu64(data[0], data[1]);

	data = fdt_getprop(dtb_address, node, "minos,ramdisk-end", &len);
	if (!data || (len == 0))
		return;
	end = fdt32_to_cpu64(data[0], data[1]);

	if (!IS_PAGE_ALIGN(start)) {
		pr_err("ramdisk region is not page align 0x%x\n", start);
		return;
	}

	split_memory_region(start, PAGE_BALIGN(end - start),
			MEMORY_REGION_TYPE_RAMDISK, 0);
}

int of_parse_memory_info(void)
{
	int node;

	node = of_get_node_by_name(dtb_address, 0, "memory");
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
	__fdt_parse_ramdisk_mem();

#ifdef CONFIG_VIRT
	__fdt_parse_vm_mem();
#endif
	return 0;
}

int of_mm_init(void)
{
	of_parse_memory_info();
	dump_memory_info();

	return 0;
}
