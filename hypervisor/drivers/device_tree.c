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
#include <minos/platform.h>
#include <minos/of.h>
#include <config/config.h>
#include <minos/virq_chip.h>

#define MAX_DTB_SIZE	(MEM_BLOCK_SIZE)

static void *dtb = NULL;
struct device_node *hv_node;
static struct vmtag *vmtags;

extern void set_vmtags_to(struct vmtag *tags, int count);

static int fdt_n_size_cells(void *dtb, int node)
{
	fdt32_t *v;
	int parent, child = node;

	parent = fdt_parent_offset(dtb, child);

	do {
		if (parent >= 0)
			child = parent;
		v = (fdt32_t *)fdt_getprop(dtb, child, "#size-cells", NULL);
		if (v)
			return fdt32_to_cpu(*v);

		parent = fdt_parent_offset(dtb, child);
	} while (parent >= 0);

	return 2;
}

static int fdt_n_addr_cells(void *dtb, int node)
{
	fdt32_t *v;
	int parent, child = node;

	parent = fdt_parent_offset(dtb, child);

	do {
		if (parent >= 0)
			child = parent;
		v = (fdt32_t *)fdt_getprop(dtb, child, "#address-cells", NULL);
		if (v)
			return fdt32_to_cpu(*v);

		parent = fdt_parent_offset(dtb, child);
	} while (parent >= 0);

	return 2;
}



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
		pr_error("can not find cpus node in dtb\n");
		return -EINVAL;
	}

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		sprintf(name, "cpu@%d", i);
		node = of_get_node_by_name(dtb, offset, name);
		if (node <= 0) {
			pr_error("can not find %s\n", name);
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

static int fdt_setup_other(struct vm *vm)
{
	int node;

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
		pr_error("fdt setup minos failed no memory\n");
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
			pr_error("fdt set interrupt for minos failed\n");
	}

	free(tmp);
	return i;
}

static int fdt_setup_cmdline(struct vm *vm)
{
	const void *data;
	int node, len, chosen_node;
	char *new_cmdline;
	char buf[512];

	chosen_node = fdt_path_offset(dtb, "/chosen");
	if (chosen_node < 0) {
		chosen_node = fdt_add_subnode(dtb, 0, "chosen");
		if (chosen_node < 0) {
			pr_error("add chosen node failed for vm0\n");
			return chosen_node;
		}
	}

	data = fdt_getprop(dtb, chosen_node, "bootargs", &len);
	if ((!data) || (len <= 0))
		pr_info("default_cmdline:\n");
	else
		pr_info("default_cmdline: %s\n", (char *)data);

	node = fdt_path_offset(dtb, "/vms/vm0");
	if (node < 0)
		return 0;

	new_cmdline = (char *)fdt_getprop(dtb, node, "cmdline", &len);
	if (!new_cmdline || len <= 0) {
		pr_info("no new cmdline using default\n");
		return 0;
	}

	/*
	 * can not directly using new_cmdline in fdt_setprop
	 * do not know why, there may a issue in libfdt or
	 * other reason
	 */
	buf[511] = 0;
	pr_info("new_cmdline:%s\n", new_cmdline);
	strncpy(buf, new_cmdline, MIN(511, len));
	fdt_setprop(dtb, chosen_node, "bootargs", buf, len);
	return 0;
}

static int fdt_setup_cpu(struct vm *vm)
{
	int offset, node, i;
	char name[16];

	/*
	 * delete unused vcpu for hvm
	 */
	offset = of_get_node_by_name(dtb, 0, "cpus");
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
	int offset, size;
	int size_cell, address_cell;
	uint32_t *args, *tmp;
	struct memory_region *region;
	unsigned long mstart, msize;


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

	list_for_each_entry(region, &mem_list, list) {
		if (region->vmid == 0) {
			pr_info("add memory region to vm0 0x%p 0x%p\n",
					region->phy_base, region->size);
			mstart = region->phy_base;
			msize = region->size;

			if (address_cell == 1) {
				*args++ = cpu_to_fdt32(mstart);
				size ++;
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
	}

	fdt_setprop(dtb, offset, "reg", (void *)tmp, size * 4);
	free(args);

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
		add_memory_region(base, size, VMID_HOST);
	}

	return 0;
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

	return 0;
}

static void parse_vm0_info(struct vmtag *tag, char *str)
{
	char *pos, *val, len;
	unsigned long value;

	pos = strchr(str, '=');
	if (pos == NULL)
		return;

	val = pos + 1;
	if (*val == 0)
		return;
	*pos = 0;

	if (strcmp(str, "vcpus") == 0) {
		value = strtoul((const char *)val, NULL, 10);
		tag->nr_vcpu = value;
	} else if (strcmp(str, "mem_base") == 0) {
		value = strtoul((const char *)val, NULL, 16);
		tag->mem_base = value;
	} else if (strcmp(str, "mem_size") == 0) {
		value = strtoul((const char *)val, NULL, 16);
		tag->mem_size = value;
	} else if (strcmp(str, "entry") == 0) {
		value = strtoul((const char *)val, NULL, 16);
		tag->entry = (void *)value;
	} else if (strcmp(str, "type") == 0) {
		len = strlen(val);
		len = MIN(len, 15);
		strncpy(tag->os_type, val, len);
		*(tag->name + len) = 0;
	} else if (strcmp(str, "name") == 0) {
		len = strlen(val);
		len = MIN(len, 15);
		strncpy(tag->name, val, len);
		*(tag->name + len) = 0;
	} else {
		pr_warn("unknown vmo argument in cmdline %s\n", str);
		return;
	}

	pr_info("vm0-%s from cmdline is %s\n", str, val);
}

static int parse_vm0_from_cmdline(struct vmtag *tag)
{
	char *str;
	char buf[512];
	int len;
	char *cmdline = of_get_cmdline(dtb);

	if (!cmdline)
		return -ENOENT;

	do {
		str = strchr(cmdline, ' ');
		if (str == NULL) {
			if (strncmp(cmdline, "minos.vm0.", 10) == 0)
				parse_vm0_info(tag, cmdline);
			else
				break;
		} else {
			len = str - cmdline;
			if ((len == 0) || len > 511)
				goto repeat;

			if (strncmp(cmdline, "minos.vm0.", 10) == 0) {
				strncpy(buf, cmdline + 10, len - 10);
				buf[len - 10] = 0;
				parse_vm0_info(tag, buf);
			}
		}
repeat:
		cmdline = str + 1;
	} while ((str != NULL) && (*str != 0));

	return 0;
}

static int __fdt_parse_vm_info(int node, struct vmtag *vmtags)
{
	char *type;
	fdt32_t *v;
	int child, index = 1, vmid, len;
	struct vmtag *tag;
	uint64_t array[2];

	if ((node <= 0) || !vmtags)
		return -EINVAL;

	fdt_for_each_subnode(child, dtb, node) {
		type = (char *)fdt_getprop(dtb, child, "device_type", &len);
		if (!type)
			continue;
		if (strcmp(type, "virtual_machine") != 0)
			continue;

		v = (fdt32_t *)fdt_getprop(dtb, child, "vmid", &len);
		if (!v)
			continue;

		vmid = fdt32_to_cpu(*v);
		if (vmid == 0)
			tag = &vmtags[0];
		else
			tag = &vmtags[index++];

		tag->vmid = vmid;
		tag->flags |= VM_FLAGS_64BIT;

		__of_get_string(dtb, child, "vm_name", tag->name, 32);
		__of_get_string(dtb, child, "type", tag->os_type, 16);
		__of_get_u32_array(dtb, child, "vcpus",
				(uint32_t *)&tag->nr_vcpu, 1);
		__of_get_u64_array(dtb, child, "entry",
				(uint64_t *)&tag->entry, 1);
		__of_get_u32_array(dtb, child, "vcpu_affinity",
				tag->vcpu_affinity, tag->nr_vcpu);
		__of_get_u32_array(dtb, child, "setup_data",
				(uint32_t *)&tag->setup_data, 1);
		__of_get_u64_array(dtb, child, "memory", array, 2);
		tag->mem_base = array[0];
		tag->mem_size = array[1];

		if (__of_get_bool(dtb, child, "vm_32bit"))
			tag->flags &= ~VM_FLAGS_64BIT;

	}

	return 0;
}

int fdt_parse_vm_info(void)
{
	char *type;
	fdt32_t *data;
	int nr_vm = 0, node, child, len, has_vm0 = 0;

	/*
	 * vm0 must be always enabled for the hypervisor
	 * first get the vm information from the dtb vms
	 * area, if no vm mentioned, then get the vm0 info
	 * from the cmdline. if not then panic
	 */
	node = fdt_path_offset(dtb, "/vms");
	if (node > 0) {
		fdt_for_each_subnode(child, dtb, node) {
			type = (char *)fdt_getprop(dtb,
					child, "device_type", &len);
			if (!type)
				continue;
			if (strcmp(type, "virtual_machine") != 0)
				continue;

			data = (fdt32_t *)fdt_getprop(dtb,
					child, "vmid", &len);
			if (data) {
				if (fdt32_to_cpu(*data) == 0)
					has_vm0++;
				nr_vm++;
			}
		}

		if (has_vm0 == 0)
			nr_vm++;
		else if (has_vm0 > 1)
			panic("detect mutiple vm0 information at the dtb\n");

		vmtags = alloc_boot_mem(sizeof(struct vmtag) * nr_vm);
		if (!vmtags)
			panic("no more memory for vmtags\n");
		memset(vmtags, 0, sizeof(struct vmtag) * nr_vm);
		__fdt_parse_vm_info(node, vmtags);
	}

	/* vmtags[0] must always be vm0 */
	parse_vm0_from_cmdline(&vmtags[0]);

	/*
	 * here all the vm information has been parsed from
	 * cmdline or the dtb, the next thing is to delete
	 * the memory region which alloctate to the vm from
	 * the system memory region
	 */
	for (len = 0; len < nr_vm; len++) {
		if (vmtags[len].mem_size != 0) {
			split_memory_region(vmtags[len].mem_base,
					vmtags[len].mem_size,
					vmtags[len].vmid);
		}
	}

	/* finally tell the system vmtags is ok */
	vmtags[0].setup_data = dtb;
	set_vmtags_to(vmtags, nr_vm);

	return 0;
}

int fdt_early_init(void *setup_data)
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

	/*
	 * set up the platform from the dtb file
	 * then get the spin table information if the
	 * platform is using spin table to wake up
	 * other cores
	 */
	fdt_setup_platform();
	fdt_parse_memory_info();
	fdt_parse_vm_info();

	return 0;
}

void fdt_vm0_init(struct vm *vm)
{
	void *fdt = vm->setup_data;

	if (!fdt)
		panic("vm0 do not have correct dtb image\n");

	fdt_open_into(fdt, fdt, MAX_DTB_SIZE);
	if(fdt_check_header(fdt)) {
		pr_error("invaild dtb after open into\n");
		return;
	}

	fdt_setup_minos(vm);
	fdt_setup_cmdline(vm);
	fdt_setup_cpu(vm);
	fdt_setup_memory(vm);
	fdt_setup_other(vm);

	if (platform->setup_hvm)
		platform->setup_hvm(vm, dtb);

	fdt_pack(dtb);
	flush_dcache_range((unsigned long)dtb, MAX_DTB_SIZE);
	destroy_host_mapping((unsigned long)dtb, MAX_DTB_SIZE);
}
