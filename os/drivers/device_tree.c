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

static void *dtb = NULL;
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

	offset = of_get_node_by_name(dtb, 0, "cpus");
	if (offset <= 0) {
		pr_err("can not find cpus node in dtb\n");
		return -EINVAL;
	}

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		sprintf(name, "cpu@%d", i);
		node = of_get_node_by_name(dtb, offset, name);
		if (node <= 0) {
			pr_err("can not find %s\n", name);
			continue;
		}

		/* get the enable methold content */
		data = fdt_getprop(dtb, node, "enable-method", &len);
		if (!data || len <= 0)
			continue;

		if (strncmp("spin-table", (char *)data, 10))
			continue;

		/* read the holding address */
		data = fdt_getprop(dtb, node, "cpu-release-addr", &len);
		if (!data || len <= sizeof(uint32_t))
			continue;

		len = len / sizeof(uint32_t);
		tmp = (uint32_t *)data;
		if (len == 1)
			smp_holding[i] = fdt32_to_cpu(tmp[0]);
		else
			smp_holding[i] = fdt32_to_cpu64(tmp[0], tmp[1]);

		pr_info("%s using spin-table relase addr 0x%p\n",
				name, smp_holding[i]);
	}

	return 0;
}

static void fdt_setup_platform(void)
{
	int len;
	const void *data;

	data = fdt_getprop(dtb, 0, "model", &len);
	if (data)
		pr_info("model : %s\n", (char *)data);

	/*
	 * compatible may like arm,fvp-base\0arm,vexpress\0
	 * but here, only parsing the first string
	 */
	data = fdt_getprop(dtb, 0, "compatible", &len);
	if (data) {
		pr_info("platform : %s\n", (char *)data);
		platform_set_to((const char *)data);
	}
}

int fdt_init(void)
{
	if (!dtb)
		panic("dtb address is not set\n");

	hv_node = of_parse_device_tree(dtb);
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

	v = (fdt32_t *)fdt_getprop(dtb, node, attr, &len);
	if (!v || (len < 8)) {
		pr_warn("no memory info found in dtb\n");
		return -ENOENT;
	}

	len = len / 4;
	size_cell = fdt_n_size_cells(dtb, node);
	address_cell = fdt_n_addr_cells(dtb, node);
	pr_info("memory node address_cells:%d size_cells:%d\n",
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
		add_memory_region(base, size, 0);
	}

	return 0;
}

static void __fdt_parse_memreserve(void)
{
#if 0
	int count, i, ret;
	uint64_t address, size;

	count = fdt_num_mem_rsv(dtb);
	if (count == 0)
		return;

	for (i = 0; i < count; i++) {
		ret = fdt_get_mem_rsv(dtb, i, &address, &size);
		if (ret)
			continue;

		pr_info("find rev memory - id: %d addr: 0x%x size: 0x%x\n",
				i, address, size);
		add_memory_region(address, size, MEMORY_REGION_F_RSV);
	}

	/* reserve the dtb memory */
	size = fdt_totalsize(dtb);
	pr_info("DTB - 0x%x ---> 0x%x\n", (unsigned long)dtb, size);
	size = BALIGN(size, PAGE_SIZE);
	add_memory_region((uint64_t)dtb, size, MEMORY_REGION_F_RSV);
#endif
}

int fdt_parse_memory_info(void)
{
	int node;

	node = of_get_node_by_name(dtb, 0, "memory");
	if (node <= 0) {
		pr_warn("no memory node found in dtb\n");
		return -ENOENT;
	}

	__fdt_parse_memory_info(node, "reg");
	__fdt_parse_memreserve();

	return 0;
}

int fdt_early_init(void *setup_data)
{
	/*
	 * the dtb file need to store at the end of the os memory
	 * region and the size can not beyond 2M, also it must
	 * 4K align, memory management will not protect this area
	 * so please put the dtb data to a right place
	 */
	if (!setup_data || !IS_PAGE_ALIGN(setup_data)) {
		pr_fatal("invalid dtb address 0x%p must 4K align\n",
				(unsigned long)setup_data);
		return -ENOMEM;
	}

	dtb = setup_data;

	if (fdt_check_header(dtb)) {
		pr_err("invaild dtb header\n");
		dtb = NULL;
		return -EINVAL;
	}

	/*
	 * set up the platform from the dtb file then get the spin
	 * table information if the platform is using spin table to
	 * wake up other cores
	 */
	fdt_setup_platform();
	fdt_parse_memory_info();

	return 0;
}

