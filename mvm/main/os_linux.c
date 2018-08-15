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

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <mvm.h>
#include <bootimage.h>
#include <libfdt/libfdt.h>
#include <vdev.h>

static int fdt_setup_commandline(void *dtb, char *cmdline)
{
	int nodeoffset;

	printv("* add command - %s\n", cmdline);

	if (!cmdline || (strlen(cmdline) == 0))
		return -EINVAL;

	nodeoffset = fdt_path_offset(dtb, "/chosen");
	if (nodeoffset < 0) {
		nodeoffset = fdt_add_subnode(dtb, 0, "chosen");
		if (nodeoffset < 0)
			return nodeoffset;
	}

	fdt_setprop(dtb, nodeoffset, "bootargs",
			cmdline, strlen(cmdline) + 1);

	return 0;
}

static int fdt_setup_cpu(void *dtb, int vcpus)
{
	int offset, node, i;
	char name[16];

	printv("* setup vcpu\n");

	/*
	 * delete unused cpu node, currently only support
	 * max 4 vcpus in one cluster, so just assume there
	 * is only one cluster node in the dtb file
	 */
	offset = fdt_path_offset(dtb, "/cpus/cpu-map/cluster0");
	if (offset < 0) {
		printf("* error - no cpu node found\n");
		return offset;
	}

	memset(name, 0, 16);
	for (i = vcpus; i < VM_MAX_VCPUS; i++) {
		sprintf(name, "core%d", i);
		node = fdt_subnode_offset(dtb, offset, name);
		if (node >= 0) {
			printv("        - delete %s\n", name);
			fdt_del_node(dtb, node);
		}
	}

	offset = fdt_path_offset(dtb, "/cpus");

	memset(name, 0, 16);
	for (i = vcpus; i < VM_MAX_VCPUS; i++) {
		sprintf(name, "cpu@%d", i);
		node = fdt_subnode_offset(dtb, offset, name);
		if (node >= 0) {
			printv("        - delete %s\n", name);
			fdt_del_node(dtb, node);
		}
	}

	return 0;
}

static int fdt_setup_memory(void *dtb, uint64_t mstart,
		uint64_t msize, int bit64)
{
	int offset, i;
	int size_cell, address_cell;
	uint32_t args[4];
	char buf[64];

	offset = fdt_num_mem_rsv(dtb);
	printv("* found %d rsv memory region\n", offset);
	for (i = 0; i < offset; i++)
		fdt_del_mem_rsv(dtb, 0);

	printv("* add rsv memory region : 0x%lx 0x%x\n",
			mstart, 0x10000);
	fdt_add_mem_rsv(dtb, mstart, 0x10000);

	memset(buf, 0, 64);
	sprintf(buf, "memory@%lx", mstart);
	offset = fdt_path_offset(dtb, "/memory");
	if (offset < 0) {
		offset = fdt_add_subnode(dtb, 0, buf);
		if (offset < 0)
			return offset;

		fdt_setprop(dtb, offset, "device_type", "memory", 7);
	} else {
		fdt_set_name(dtb, offset, buf);
	}

	size_cell = fdt_size_cells(dtb, offset);
	address_cell = fdt_address_cells(dtb, offset);
	if (bit64) {
		if ((size_cell != 2) && (address_cell != 2))
			return -EINVAL;

		args[0] = cpu_to_fdt32(mstart >> 32);
		args[1] = cpu_to_fdt32(mstart);
		args[2] = cpu_to_fdt32(msize >> 32);
		args[3] = cpu_to_fdt32(msize);
		size_cell = sizeof(uint32_t) * 4;
	} else {
		args[0] = cpu_to_fdt32(mstart);
		args[1] = cpu_to_fdt32(msize);
		size_cell = sizeof(uint32_t) * 2;
	}

	return fdt_setprop(dtb, offset, "reg",
			(void *)args, size_cell);
}


static int fdt_setup_ramdisk(void *dtb, uint32_t start, uint32_t size)
{
	int offset;

	offset = fdt_path_offset(dtb, "/chosen");
	if (offset < 0) {
		offset = fdt_add_subnode(dtb, 0, "chosen");
		if (offset < 0)
			return offset;
	}

	printv("* set ramdisk : 0x%x 0x%x", start, size);
	fdt_setprop_cell(dtb, offset, "linux,initrd-start", start);
	fdt_setprop_cell(dtb, offset, "linux,initrd-end", start + size);

	return 0;
}

static int linux_setup_env(struct vm *vm)
{
	int ret = 0;
	void *vbase;
	uint32_t pages, seek, offset, load_size;
	struct vm_info *info = &vm->vm_info;
	boot_img_hdr *hdr = (boot_img_hdr *)vm->os_data;

	if (hdr->second_size == 0)
		return -EINVAL;

	pages = BALIGN(hdr->kernel_size, hdr->page_size) +
		BALIGN(hdr->ramdisk_size, hdr->page_size);
	pages = (pages / hdr->page_size) + 1;
	seek = pages * hdr->page_size;
	load_size = BALIGN(hdr->second_size, hdr->page_size);
	offset = hdr->second_addr - info->mem_start;

	printv("dtb-0x%x 0x%x 0x%x\n", seek, load_size, offset);

	if (lseek(vm->image_fd, seek, SEEK_SET) == -1)
		return -EIO;

	/* the max size of dtb region is 2M */
	vbase = malloc(MEM_BLOCK_SIZE);
	if (!vbase)
		return -ENOMEM;

	ret = read(vm->image_fd, vbase, load_size);
	if (ret <= 0) {
		printf("* error - read dtb image failed\n");
		ret = -EIO;
		goto free_mem;
	}

	if (fdt_check_header(vbase)) {
		printf("* error - invalid DTB please check the bootimage\n");
		ret = -EINVAL;
		goto free_mem;
	}

	fdt_open_into(vbase, vbase, 0x200000);
	if (fdt_check_header(vbase)) {
		printf("* error - invalid DTB after open into\n");
		ret = -EINVAL;
		goto free_mem;
	}

	/*
	 * 1: set up commandline
	 * 2: set up nr vcpu
	 * 3: setup mem attr
	 */
	fdt_setup_commandline(vbase, (char *)hdr->cmdline);
	fdt_setup_cpu(vbase, info->nr_vcpus);
	fdt_setup_memory(vbase, info->mem_start,
			info->mem_size, info->bit64);

	if (!(vm->flags & (MVM_FLAGS_NO_RAMDISK)))
		fdt_setup_ramdisk(vbase, hdr->ramdisk_addr,
				hdr->ramdisk_size);

	/* call the vdev call back */
	vdev_setup_env(vm, vbase, OS_TYPE_LINUX);

	fdt_pack(vbase);

	ret = 0;
	memcpy(vm->mmap + offset, vbase, MEM_BLOCK_SIZE);

free_mem:
	free(vbase);
	return ret;
}

static int load_image(struct vm *vm, uint32_t load_offset,
		uint32_t image_offset, uint32_t load_size)
{
	int ret;
	int image_fd = vm->image_fd;

	if (lseek(image_fd, image_offset, SEEK_SET) == -1)
		return -EIO;

	ret = read(image_fd, (char *)(vm->mmap + load_offset), load_size);
	if (ret <= 0)
		return ret;

	return 0;
}

static int linux_load_image(struct vm *vm)
{
	int ret = 0;
	struct vm_info *info = &vm->vm_info;
	uint32_t load_size, load_offset, seek, tmp = 1;
	boot_img_hdr *hdr = (boot_img_hdr *)vm->os_data;

	/* load the kernel image to guest memory */
	load_offset = hdr->kernel_addr - info->mem_start;
	load_size = hdr->kernel_size;
	seek = tmp * hdr->page_size;

	printv("* load kernel image: 0x%x 0x%x 0x%x\n",
			load_offset, seek, load_size);

	ret = load_image(vm, load_offset, seek, load_size);
	if (ret) {
		perror("* error - load kernel image failed\n");
		return ret;
	}

	tmp += (hdr->kernel_size + hdr->page_size - 1) / hdr->page_size;

	/* load the ramdisk image */
	if (vm->flags & (MVM_FLAGS_NO_RAMDISK))
		return 0;

	load_offset = hdr->ramdisk_addr - info->mem_start;
	load_size = hdr->ramdisk_size;
	seek = tmp * hdr->page_size;

	printv("* load ramdisk image:0x%x 0x%x 0x%x\n",
			load_offset, seek, load_size);
	ret = load_image(vm, load_offset, seek, load_size);
	if (ret) {
		perror("* error - load ramdisk image failed\n");
		return ret;
	}

	return 0;
}

static int linux_early_init(struct vm *vm)
{
	int ret;
	boot_img_hdr *hdr;
	struct vm_info *info = &vm->vm_info;

	if (vm->image_fd <= 0)
		return -EINVAL;

	hdr = (boot_img_hdr *)malloc(sizeof(boot_img_hdr));
	if (!hdr)
		return -ENOMEM;

	ret = read_bootimage_header(vm->image_fd, hdr);
	if (ret) {
		printf("* error - image is not a vaild bootimage\n");
		return ret;
	}

	info->entry = hdr->kernel_addr;
	info->setup_data = hdr->second_addr;
	info->mem_start = info->entry & ~(0x200000 - 1);
	vm->os_data = (void *)hdr;

	return 0;
}

struct vm_os os_linux = {
	.name	    	= "linux",
	.early_init 	= linux_early_init,
	.load_image 	= linux_load_image,
	.setup_vm_env   = linux_setup_env,
};

DEFINE_OS(os_linux);
