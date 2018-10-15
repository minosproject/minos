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
#include <minos/vmm.h>
#include <libfdt/libfdt.h>
#include <minos/vm.h>
#include <minos/virt.h>

#define MAX_DTB_SIZE	(MEM_BLOCK_SIZE)

static void *dtb = NULL;

static void *of_getprop(char *path, char *attr, int *len)
{
	const void *data;
	int offset;

	if (!dtb)
		return NULL;

	offset = fdt_path_offset(dtb, path);
	if (offset < 0) {
		pr_error("no such node %d\n", path);
		return NULL;
	}

	data = fdt_getprop(dtb, offset, attr, len);
	if (!data || (*len == 0)) {
		pr_error("can not get %s in %s error-%d\n",
				attr, path, *len);
		return NULL;
	}

	return (void *)data;
}

const char *of_get_compatible(int node)
{
	const void *data;
	int len;

	data = fdt_getprop(dtb, node, "compatible", &len);
	if (!data || len == 0)
		return NULL;

	return (const char *)data;
}

int of_get_u32_array(char *path, char *attr, uint32_t *array, int *len)
{
	void *data;
	fdt32_t *val;
	int length, i;

	data = of_getprop(path, attr, &length);
	if (!data)
		return -EINVAL;

	if (length & (sizeof(uint32_t) - 1)) {
		pr_error("node is not a u64 array %d\n", length);
		return -EINVAL;
	}

	val = (fdt32_t *)data;
	*len = 0;
	for (i = 0; i < (length / sizeof(fdt32_t)); i++)
		*array++ = fdt32_to_cpu(val[i]);

	*len = length / sizeof(uint32_t);

	return 0;
}

int of_get_u64_array(char *path, char *attr, uint64_t *array, int *len)
{
	void *data;
	fdt32_t *val;
	int length, i;

	data = of_getprop(path, attr, &length);
	if (!data) {
		pr_error("can not get %s in %s error-%d\n",
				attr, path, length);
		return -EINVAL;
	}

	if (length & (sizeof(uint64_t) - 1)) {
		pr_error("node is not a u64 array %d\n", length);
		return -EINVAL;
	}

	val = (fdt32_t *)data;
	*len = 0;
	for (i = 0; i < (length / sizeof(fdt32_t)); i += 2) {
		*array++ = (uint64_t)(fdt32_to_cpu(val[i])) << 32 |
			fdt32_to_cpu(val[i + 1]);
	}

	*len = length / sizeof(uint64_t);

	return 0;
}

int of_get_node_by_name(int pnode, char *str, int deepth)
{
	const char *name;
	int node = -1, child, len;

	if (deepth >= 10)
		return -ENOENT;

	fdt_for_each_subnode(node, dtb, pnode) {
		if (NULL == (name = fdt_get_name(dtb, node, &len)))
			continue;
		if (len < 0)
			continue;

		if (strncmp(name, str, strlen(str)) == 0)
			return node;
		else {
			child = of_get_node_by_name(node, str, deepth + 1);
			if (child > 0)
				return child;
		}
	}

	return -ENOENT;
}

int of_get_interrupt_regs(int node, uint64_t *array, int *array_len)
{
	const void *data;
	uint32_t *u32_data;
	int len, size_cell, i;

	if (node <= 0)
		return -EINVAL;

	size_cell = fdt_size_cells(dtb, node);
	if ((size_cell != 1) && (size_cell != 2)) {
		pr_error("invaild size cell of interrupt node\n");
		return -EINVAL;
	}

	data = fdt_getprop(dtb, node, "reg", &len);
	if (!data || (len <= 0))
		return -EINVAL;

	/*
	 * Fix Me: espressobin dts's size_cell of interrupt
	 * controller is 2, but the reg is using uint32_t
	 * format, so the length will be wrong
	 */
	u32_data = (uint32_t *)data;
	len = (len / sizeof(uint32_t));
	if (*array_len < (len / size_cell))
		return -EINVAL;
	else {
#ifndef CONFIG_PLATFORM_ARMADA
		*array_len = len / size_cell;
#else
		*array_len = len;
#endif
	}

	if (size_cell == 1) {
		for (i = 0; i < len; i++)
			array[i] = fdt32_to_cpu(u32_data[i]);
	} else {
#ifndef CONFIG_PLATFORM_ARMADA
		if (len & 1)
			return -EINVAL;

		for (i = 0; i < len; i += 2) {
			array[i / 2] = (uint64_t)(fdt32_to_cpu(u32_data[i])) << 32 |
				fdt32_to_cpu(u32_data[i + 1]);
		}
#else
		for (i = 0; i < len; i++)
			array[i] = fdt32_to_cpu(u32_data[i]);
#endif
	}

	for (i = 0; i < len; i += 2)
		array[i] = array[i] + CONFIG_PLATFORM_IO_BASE;

	return 0;
}

int of_init(void *setup_data)
{
	unsigned long base;

	base = (unsigned long)setup_data;
	pr_info("dtb address is 0x%x\n", (unsigned long)base);

	if (!setup_data || (base & (MEM_BLOCK_SIZE - 1))) {
		pr_fatal("invalid dtb address\n");
		return -ENOMEM;
	}

	/* map the dtb space to hypervisor's mem space */
	if (create_early_pmd_mapping(base, base)) {
		pr_error("map dtb memory failed\n");
		return -EINVAL;
	}

	dtb = setup_data;

	if (fdt_check_header(dtb)) {
		pr_error("invaild dtb header\n");
		dtb = NULL;
		return -EINVAL;
	}

	return 0;
}

static int fdt_setup_minos(struct vm *vm)
{
	int node, i;
	uint32_t *array = NULL, *tmp;
	size_t size;
	int vspi_nr = vm->vspi_nr;

	node = fdt_path_offset(dtb, "/minos");
	if (node < 0) {
		node = fdt_add_subnode(dtb, 0, "minos");
		if (node < 0)
			return node;
	}

	size = vspi_nr * sizeof(uint32_t) * 3;
	size = PAGE_BALIGN(size);
	tmp = array = (uint32_t *)get_free_pages(PAGE_NR(size));
	size = 0;
	if (!array) {
		pr_error("fdt setup minos failed no memory\n");
		return -ENOMEM;
	}

	fdt_setprop(dtb, node, "compatible", "minos,hypervisor", 17);

	for (i = 0; i < vspi_nr; i++) {
		*array++ = cpu_to_fdt32(0);
		*array++ = cpu_to_fdt32(i);
		*array++ = cpu_to_fdt32(4);
		size += (3 * sizeof(uint32_t));
	}

	i = fdt_setprop(dtb, node, "interrupts", (void *)tmp, size);
	if (i)
		pr_error("fdt set interrupt for minos failed\n");

	free(tmp);

	return i;
}

static int fdt_setup_cmdline(struct vm *vm)
{
	const void *data;
	int node, len;
	char *new_cmdline;

	node = fdt_path_offset(dtb, "/chosen");
	if (node < 0) {
		node = fdt_add_subnode(dtb, 0, "chosen");
		if (node < 0) {
			pr_error("add chosen node failed for vm0\n");
			return node;
		}
	}

	data = fdt_getprop(dtb, node, "bootargs", &len);
	if ((!data) || (len <= 0))
		pr_info("default_cmdline:");
	else
		pr_info("default_cmdline: %s\n", (char *)data);

	new_cmdline = mv_config->vmtags[0].cmdline;
	len = strlen(new_cmdline);
	if (len == 0) {
		pr_info("no new cmdline using default\n");
		return 0;
	} else
		pr_info("new_cmdline:%s\n", new_cmdline);

	fdt_setprop(dtb, node, "bootargs", new_cmdline, len);

	return 0;
}

static int fdt_setup_cpu(struct vm *vm)
{
	int offset, node, i;
	char name[16];

	/*
	 * delete unused vcpu for hvm
	 */
	offset = of_get_node_by_name(0, "cpus", 0);
	if (offset < 0) {
		pr_error("can not find cpus node in dtb\n");
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
	int offset;
	int size_cell, address_cell;
	uint32_t *args, *tmp;
	struct memory_region *region;
	unsigned long mstart, msize;

	offset = fdt_path_offset(dtb, "/memory");
	if (offset < 0) {
		offset = fdt_add_subnode(dtb, 0, "memory");
		if (offset < 0)
			return offset;

		fdt_setprop(dtb, offset, "device_type", "memory", 7);
	}

	size_cell = fdt_size_cells(dtb, offset);
	address_cell = fdt_address_cells(dtb, offset);

	if ((size_cell != 2) && (address_cell != 2))
		return -EINVAL;

	tmp = args = (uint32_t *)get_free_page();
	if (!args)
		return -ENOMEM;

	size_cell = 0;

	list_for_each_entry(region, &mem_list, list) {
		if ((region->type == MEM_TYPE_NORMAL) &&
				(region->vmid == 0)) {
			pr_info("add memory region to vm0 0x%p 0x%p\n",
					region->phy_base, region->size);
			mstart = region->phy_base;
			msize = region->size;
			*args++ = cpu_to_fdt32(mstart >> 32);
			*args++ = cpu_to_fdt32(mstart);
			*args++ = cpu_to_fdt32(msize >> 32);
			*args++ = cpu_to_fdt32(msize);
			size_cell += sizeof(uint32_t) * 4;
		}
	}

	fdt_setprop(dtb, offset, "reg", (void *)tmp, size_cell);
	free(args);

	return 0;
}

static int fdt_setup_timer(struct vm *vm)
{
#ifdef CONFIG_PLATFORM_FVP
	int node;
	int len;
	const void *val;

	/* disable the armv7-timer-mem for fvp*/
	node = fdt_path_offset(dtb, "/timer@2a810000");
	if (node < 0)
		return 0;

	val = fdt_getprop(dtb, node, "compatible", &len);
	if (!val || len <= 0)
		return 0;

	if (!strcmp((char *)val, "arm,armv7-timer-mem")) {
		pr_info("delete the armv7 mem timer\n");
		fdt_del_node(dtb, node);
	}
#endif

	return 0;
}

void hvm_dtb_init(struct vm *vm)
{
	if (!dtb)
		return;

	fdt_open_into(dtb, dtb, MAX_DTB_SIZE);
	if(fdt_check_header(dtb)) {
		pr_error("invaild dtb after open into\n");
		return;
	}

	/* update the dtb address for the hvm */
	vm->setup_data = (unsigned long)dtb;

	fdt_setup_minos(vm);
	fdt_setup_cmdline(vm);
	fdt_setup_cpu(vm);
	fdt_setup_memory(vm);
	fdt_setup_timer(vm);

	fdt_pack(dtb);
	flush_dcache_range((unsigned long)dtb, MAX_DTB_SIZE);
}
