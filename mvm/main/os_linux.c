/*
 * BSD 3-Clause License
 *
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include <sys/stat.h>

#include <mvm.h>
#include <bootimage.h>
#include <libfdt/libfdt.h>
#include <vdev.h>

#define GIC_TYPE_GICV3	(0)
#define GIC_TYPE_GICV2	(1)
#define GIC_TYPE_GICV4	(2)

static int fdt_set_gicv2(void *dtb, int node)
{
	uint32_t regs[16];

	pr_info("vm gic type is gic-v2\n");
	fdt_setprop(dtb, node, "compatible",
			"arm,cortex-a15-gic", 19);

	/* gicd */
	regs[0] = cpu_to_fdt32(0);
	regs[1] = cpu_to_fdt32(0x2f000000);
	regs[2] = cpu_to_fdt32(0x0);
	regs[3] = cpu_to_fdt32(0x10000);

	/* gicc */
	regs[4] = cpu_to_fdt32(0);
	regs[5] = cpu_to_fdt32(0x2c000000);
	regs[6] = cpu_to_fdt32(0x0);
	regs[7] = cpu_to_fdt32(0x2000);

	/* gich */
	regs[8] = cpu_to_fdt32(0);
	regs[9] = cpu_to_fdt32(0x2c010000);
	regs[10] = cpu_to_fdt32(0x0);
	regs[11] = cpu_to_fdt32(0x2000);

	/* gicv */
	regs[12] = cpu_to_fdt32(0);
	regs[13] = cpu_to_fdt32(0x2c02f000);
	regs[14] = cpu_to_fdt32(0x0);
	regs[15] = cpu_to_fdt32(0x2000);
	fdt_setprop(dtb, node, "reg", (void *)regs, 64);

	regs[0] = cpu_to_fdt32(1);
	regs[1] = cpu_to_fdt32(9);
	regs[2] = cpu_to_fdt32(4);
	fdt_setprop(dtb, node, "interrupts", (void *)regs, 12);

	return 0;
}

static int fdt_set_gicv3(void *dtb, int node)
{
	uint32_t regs[20];
	int its_node;

	pr_info("vm gic type is gic-v3\n");
	fdt_setprop(dtb, node, "compatible", "arm,gic-v3", 11);

	/* gicd */
	regs[0] = cpu_to_fdt32(0x0);
	regs[1] = cpu_to_fdt32(0x2f000000);
	regs[2] = cpu_to_fdt32(0x0);
	regs[3] = cpu_to_fdt32(0x10000);

	/* gicr */
	regs[4] = cpu_to_fdt32(0x0);
	regs[5] = cpu_to_fdt32(0x2f100000);
	regs[6] = cpu_to_fdt32(0x0);
	regs[7] = cpu_to_fdt32(0x200000);

	/* gicc */
	regs[8] = cpu_to_fdt32(0x0);
	regs[9] = cpu_to_fdt32(0x2c000000);
	regs[10] = cpu_to_fdt32(0x0);
	regs[11] = cpu_to_fdt32(0x2000);

	/* gich */
	regs[12] = cpu_to_fdt32(0x0);
	regs[13] = cpu_to_fdt32(0x2c010000);
	regs[14] = cpu_to_fdt32(0x0);
	regs[15] = cpu_to_fdt32(0x2000);

	/* gicv */
	regs[16] = cpu_to_fdt32(0x0);
	regs[17] = cpu_to_fdt32(0x2c02f000);
	regs[18] = cpu_to_fdt32(0x0);
	regs[19] = cpu_to_fdt32(0x2000);
	fdt_setprop(dtb, node, "reg", (void *)regs, 80);

	regs[0] = cpu_to_fdt32(1);
	regs[1] = cpu_to_fdt32(9);
	regs[2] = cpu_to_fdt32(4);
	fdt_setprop(dtb, node, "interrupts", (void *)regs, 12);

	/* add the its node */
	its_node = fdt_add_subnode(dtb, node, "its@2f020000");
	if (its_node < 0) {
		pr_err("add its node for gicv3 failed\n");
		return its_node;
	}

	fdt_setprop(dtb, its_node, "compatible",
			"arm,gic-v3-its", 15);
	fdt_setprop(dtb, its_node, "msi-controller", "", 0);

	regs[0] = cpu_to_fdt32(0x0);
	regs[1] = cpu_to_fdt32(0x2f020000);
	regs[2] = cpu_to_fdt32(0x0);
	regs[3] = cpu_to_fdt32(0x20000);
	fdt_setprop(dtb, its_node, "reg", (void *)regs, 12);

	return 0;
}

static int fdt_set_gicv4(void *dtb, int node)
{
	pr_info("vm gic type is gic-v4\n");
	return 0;
}

static int fdt_set_gic(void *dtb, int gic_type)
{
	int node;

	node = fdt_path_offset(dtb, "/interrupt-controller@2f000000");
	if (node < 0) {
		node = fdt_add_subnode(dtb, 0, "interrupt-controller");
		if (node < 0) {
			pr_err("add interrupt node failed\n");
			return node;
		}
	}

	switch (gic_type) {
	case GIC_TYPE_GICV2:
		fdt_set_gicv2(dtb, node);
		break;
	case GIC_TYPE_GICV3:
		fdt_set_gicv3(dtb, node);
		break;
	case GIC_TYPE_GICV4:
		fdt_set_gicv4(dtb, node);
		break;
	default:
		pr_warn("unsupport gic version:%d now, using gicv3\n",
				gic_type);
		fdt_set_gicv3(dtb, node);
		break;
	}

	return 0;
}

static int fdt_setup_commandline(void *dtb, char *cmdline)
{
	int nodeoffset;

	pr_info("add cmdline - %s\n", cmdline);

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

	/*
	 * delete unused cpu node, currently only support
	 * max 4 vcpus in one cluster, so just assume there
	 * is only one cluster node in the dtb file
	 */
	offset = fdt_path_offset(dtb, "/cpus/cpu-map");
	if (offset > 0) {
		pr_info("delete cpu-map node\n");
		fdt_del_node(dtb, offset);
	}

	offset = fdt_path_offset(dtb, "/cpus");

	memset(name, 0, 16);
	for (i = vcpus; i < VM_MAX_VCPUS; i++) {
		sprintf(name, "cpu@%d", i);
		node = fdt_subnode_offset(dtb, offset, name);
		if (node >= 0) {
			pr_info("        - delete %s\n", name);
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
	pr_debug("found %d rsv memory region\n", offset);
	for (i = 0; i < offset; i++)
		fdt_del_mem_rsv(dtb, 0);

	pr_debug("add rsv memory region : 0x%lx 0x%x\n",
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
		pr_info("setup memory 0x%x 0x%x 0x%x 0x%x\n",
				args[0], args[1], args[2], args[3]);
	} else {
		args[0] = cpu_to_fdt32(mstart);
		args[1] = cpu_to_fdt32(msize);
		size_cell = sizeof(uint32_t) * 2;
		pr_info("setup memory 0x%x 0x%x\n", args[0], args[1]);
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

	pr_info("set ramdisk : 0x%x 0x%x\n", start, size);
	fdt_setprop_cell(dtb, offset, "linux,initrd-start", start);
	fdt_setprop_cell(dtb, offset, "linux,initrd-end", start + size);

	return 0;
}

static int linux_setup_env(struct vm *vm, char *cmdline)
{
	char *arg;
	boot_img_hdr *hdr = (boot_img_hdr *)vm->os_data;
	void *vbase = vm->mmap + (vm->setup_data - vm->mem_start);

	if (fdt_check_header(vbase)) {
		pr_err("invalid DTB please check the bootimage\n");
		return -EINVAL;
	}

	fdt_open_into(vbase, vbase, MEM_BLOCK_SIZE);
	if (fdt_check_header(vbase)) {
		pr_err("invalid DTB after open into\n");
		return -EINVAL;
	}

	/*
	 * 1: set up commandline
	 * 2: set up nr vcpu
	 * 3: setup mem attr
	 */
	if (cmdline && (strlen(cmdline) > 0))
		arg = cmdline;
	else {
		if (!(vm->flags & MVM_FLAGS_NO_BOOTIMAGE))
			arg = (char *)hdr->cmdline;
	}

	fdt_setup_commandline(vbase, arg);
	fdt_setup_cpu(vbase, vm->nr_vcpus);
	fdt_setup_memory(vbase, vm->mem_start, vm->mem_size, vm->bit64);
	fdt_set_gic(vbase, vm->vm_config->gic_type);

	if (!(vm->flags & (MVM_FLAGS_NO_RAMDISK))) {
		if (vm->flags & MVM_FLAGS_NO_BOOTIMAGE) {
			struct stat stbuf;
			if (fstat(vm->rfd, &stbuf) != 0)
				return -EINVAL;
			fdt_setup_ramdisk(vbase, 0x83000000, stbuf.st_size);
		} else
			fdt_setup_ramdisk(vbase, hdr->ramdisk_addr,
					hdr->ramdisk_size);
	}

	/* call the vdev call back */
	vdev_setup_env(vm, vbase, OS_TYPE_LINUX);

	fdt_pack(vbase);

	return 0;
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

static int linux_load_bootimage(struct vm *vm)
{
	int ret = 0;
	uint32_t load_size, pages, load_offset, seek, tmp = 1;
	boot_img_hdr *hdr = (boot_img_hdr *)vm->os_data;

	/* load the kernel image to guest memory */
	load_offset = hdr->kernel_addr - vm->mem_start;
	load_size = hdr->kernel_size;
	seek = tmp * hdr->page_size;

	pr_debug("load kernel image: 0x%x 0x%x 0x%x\n",
			load_offset, seek, load_size);

	ret = load_image(vm, load_offset, seek, load_size);
	if (ret) {
		pr_err("error - load kernel image failed\n");
		return ret;
	}

	tmp += (hdr->kernel_size + hdr->page_size - 1) / hdr->page_size;

	/* load the ramdisk image */
	if (vm->flags & (MVM_FLAGS_NO_RAMDISK))
		goto load_dtb;

	load_offset = hdr->ramdisk_addr - vm->mem_start;
	load_size = hdr->ramdisk_size;
	seek = tmp * hdr->page_size;

	pr_debug("load ramdisk image:0x%x 0x%x 0x%x\n",
			load_offset, seek, load_size);
	ret = load_image(vm, load_offset, seek, load_size);
	if (ret) {
		pr_err("load ramdisk image failed\n");
		return ret;
	}

load_dtb:
	if (hdr->second_size == 0)
		return -EINVAL;

	pages = BALIGN(hdr->kernel_size, hdr->page_size) +
		BALIGN(hdr->ramdisk_size, hdr->page_size);
	pages = (pages / hdr->page_size) + 1;
	seek = pages * hdr->page_size;
	load_size = BALIGN(hdr->second_size, hdr->page_size);
	load_offset = hdr->second_addr - vm->mem_start;
	ret = load_image(vm, load_offset, seek, load_size);
	if (ret) {
		pr_err("load dtb image failed\n");
		return ret;
	}

	return 0;
}

static int load_spare_image(int fd, void *target)
{
	size_t size;
	struct stat stbuf;

	if (fd < 0)
		return -EINVAL;

	if ((fstat(fd, &stbuf) != 0) ||
			(!S_ISREG(stbuf.st_mode)))
		return -EINVAL;

	size = stbuf.st_size;
	pr_debug("load 0x%lx to 0x%lx\n", size, (unsigned long)target);

	if (read(fd, target, size) != size)
		return -EIO;

	lseek(fd, 0, SEEK_SET);

	return 0;
}

static int linux_load_images(struct vm *vm)
{
	int ret;
	void *base = vm->mmap;

	if ((vm->kfd <= 0) || (vm->dfd <= 0))
		return -EINVAL;

	ret = load_spare_image(vm->kfd, base + 0x80000);
	if (ret) {
		pr_err("read kernel image failed\n");
		return -EIO;
	}

	ret = load_spare_image(vm->dfd, base + 0x3e00000);
	if (ret) {
		pr_err("read dtb image failed\n");
		return -EIO;
	}

	if (vm->flags & MVM_FLAGS_NO_RAMDISK)
		return 0;

	ret = load_spare_image(vm->rfd, base + 0x3000000);
	if (ret) {
		pr_err("read the ramdisk image failed\n");
		return -EIO;
	}

	return 0;
}

static int linux_load_image(struct vm *vm)
{
	if (vm->flags & MVM_FLAGS_NO_BOOTIMAGE)
		return linux_load_images(vm);
	else
		return linux_load_bootimage(vm);
}

static int linux_parse_bootimage(struct vm *vm)
{
	int ret;
	boot_img_hdr *hdr;

	if (vm->image_fd <= 0)
		return -EINVAL;

	hdr = (boot_img_hdr *)malloc(sizeof(boot_img_hdr));
	if (!hdr)
		return -ENOMEM;

	ret = read_bootimage_header(vm->image_fd, hdr);
	if (ret) {
		pr_err("image is not a vaild bootimage\n");
		return ret;
	}

	vm->entry = hdr->kernel_addr;
	vm->setup_data = hdr->second_addr;
	vm->mem_start = vm->entry & ~(0x200000 - 1);
	vm->os_data = (void *)hdr;

	return 0;

}

static int linux_parse_image(struct vm *vm)
{
	/* the images will loaded at fixed address */
	vm->entry = 0x80080000;
	vm->setup_data = 0x83e00000;
	vm->mem_start = 0x80000000;

	return 0;
}

static int linux_early_init(struct vm *vm)
{
	int ret = 0;

	if (vm->flags & MVM_FLAGS_NO_BOOTIMAGE)
		ret = linux_parse_image(vm);
	else
		ret = linux_parse_bootimage(vm);

	return ret;
}

struct vm_os os_linux = {
	.name	    	= "linux",
	.early_init 	= linux_early_init,
	.load_image 	= linux_load_image,
	.setup_vm_env   = linux_setup_env,
};

DEFINE_OS(os_linux);
