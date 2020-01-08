/*
 * BSD 3-Clause License
 *
 * Copyright (C) 2019 Min Le (lemin9538@gmail.com)
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
#include <os_xnu.h>

#define XNU_KERNEL_BASE		0x40000000
#define VA_OFFSET(addr)		((addr) & 0x3fffffff)
#define VA2PA(addr)		(XNU_KERNEL_BASE + ((addr) & 0x3fffffff))

struct xnu_segment_cmd64 {
	struct segment_cmd64 *cmd;
	struct xnu_segment_cmd64 *next;
};

struct xnu_os_data {
	uint64_t entry_point;
	uint64_t kernel_load_base;
	uint64_t load_end;
	uint64_t ramdisk_load_base;
	uint64_t ramdisk_size;
	uint64_t dtb_load_base;
	uint64_t dtb_size;
	uint64_t bootarg_load_base;
	void *meta_buf;
	struct xnu_segment_cmd64 *root;
};

static void xnu_vm_exit(struct vm *vm)
{

}

static int xnu_setup_env(struct vm *vm, char *cmdline)
{
	size_t i;
	uint64_t *value;
	struct xnu_dt_node_prop *dt_prop;
	struct xnu_os_data *od = (struct xnu_os_data *)vm->os_data;
	struct xnu_arm64_boot_args *arg = (struct xnu_arm64_boot_args *)
		(vm->mmap + VA_OFFSET(od->bootarg_load_base));
	char *dtb = (char *)(vm->mmap + VA_OFFSET(od->dtb_load_base));

	memset(arg, 0, sizeof(struct xnu_arm64_boot_args));
	arg->revision = XNU_ARM64_KBOOT_ARGS_REVISION2;
	arg->version = XNU_ARM64_KBOOT_ARGS_VERSION;
	arg->virt_base = od->kernel_load_base & ~0x3fffffff;
	arg->phys_base = XNU_KERNEL_BASE;
	arg->mem_size = vm->mem_size;			/* fix 1GB */
	arg->top_of_kdata = VA2PA(od->load_end);
	arg->dtb = VA2PA(od->ramdisk_load_base);
	arg->dtb_length = od->dtb_size;
	arg->mem_size_actual = 0;
	arg->boot_flags = 0;
	strncpy(arg->cmdline, cmdline, strlen(cmdline) + 1);

	/*
	 * add the ramdisk information to the dtb, just search
	 * the string "MemoryMapReserved-0"
	 */
	for (i = 0; i < od->dtb_size; i++) {
		if (strncmp(dtb + i, "MemoryMapReserved-0",
				XNU_DT_PROP_NAME_LENGTH) == 0) {
			dt_prop = (struct xnu_dt_node_prop *)dtb;
			break;
		}
	}

	if (!dt_prop) {
		pr_err("Can't find the ramdisk node\n");
		return -ENOENT;
	}

	strncpy(dt_prop->name, "RAMDisk", XNU_DT_PROP_NAME_LENGTH);
	value = (uint64_t *)&dt_prop->value;
	value[0] = VA2PA(od->ramdisk_load_base);
	value[1] = od->ramdisk_size;

	return 0;
}

static int inline xnu_load_raw_data(int fd, void *base,
		uint64_t offset, uint64_t file_off, uint64_t file_size)
{
	int ret;

	pr_info("load image: 0x%p 0x%lx 0x%lx 0x%lx\n", base, offset, file_off, file_size);
	if (file_size == 0)
		return 0;

	if (lseek(fd, file_off, SEEK_SET) == -1)
		return -EIO;

	ret = read(fd, base + offset, file_size);
	if (ret <= 0)
		return ret;

	return 0;
}

static int xnu_load_kernel_image(struct vm *vm)
{
	int ret;
	void *vm_base = vm->mmap;
	struct xnu_os_data *od = (struct xnu_os_data *)vm->os_data;
	struct xnu_segment_cmd64 *root = od->root;
	struct segment_cmd64 *cmd;

	if (vm->kfd <= 0)
		return -ENOENT;

	while (root) {
		cmd = root->cmd;
		ret = xnu_load_raw_data(vm->kfd, vm_base, VA_OFFSET(cmd->vm_addr),
				cmd->file_off, cmd->file_size);
		if (ret)
			return ret;

		root = root->next;
	}

	return 0;
}

static int xnu_load_ramdisk(struct vm *vm)
{
	struct xnu_os_data *od = (struct xnu_os_data *)vm->os_data;

	if (vm->rfd <= 0)
		return -ENOENT;

	return xnu_load_raw_data(vm->rfd, vm->mmap,
			VA_OFFSET(od->ramdisk_load_base), 0, od->ramdisk_size);
}

static int xnu_load_dtb(struct vm *vm)
{
	struct xnu_os_data *od = (struct xnu_os_data *)vm->os_data;

	if (vm->dfd <= 0)
		return -ENOENT;

	return xnu_load_raw_data(vm->dfd, vm->mmap,
			VA_OFFSET(od->dtb_load_base), 0, od->dtb_size);
}

static int xnu_load_image(struct vm *vm)
{
	int ret = 0;

	/*
	 * usually ios also have 3 images these are kernel
	 * ramdisk and device_tree, for xnu can not use bootimge
	 */
	ret += xnu_load_kernel_image(vm);
	ret += xnu_load_ramdisk(vm);
	ret += xnu_load_dtb(vm);

	return ret;
}

static void inline xnu_dump_cmd64(struct segment_cmd64 *cmd64)
{
	pr_info("\n");
	pr_debug("segname %s\n", cmd64->seg_name);
	pr_debug("vm_addr 0x%lx\n", cmd64->vm_addr);
	pr_debug("vm_size %ld\n", cmd64->vm_size);
	pr_debug("file_off %ld\n", cmd64->file_off);
	pr_debug("file_size %ld\n", cmd64->file_size);
	pr_debug("max_port %d\n", cmd64->max_port);
	pr_debug("init_port %d\n", cmd64->init_port);
	pr_debug("nsects %d\n", cmd64->nsects);
	pr_debug("flags 0x%x\n", cmd64->flags);
	pr_info("\n");
}

static void xnu_new_cmd64(struct xnu_os_data *od, struct segment_cmd64 *cmd64)
{
	struct xnu_segment_cmd64 *cmd;

	cmd = malloc(sizeof(struct xnu_segment_cmd64));
	if (!cmd) {
		pr_err("no more memory for segment64\n");
		return;
	}

	cmd->cmd = cmd64;
	cmd->next = od->root;

	od->root = cmd;
}

static int xnu_parse_kernel_image(int fd, struct xnu_os_data *od)
{
	int ret, i;
	char *cmds;
	struct mach_hdr64 hdr;
	struct load_cmd *cmd;
	struct segment_cmd64 *cmd64;

	ret = read(fd, &hdr, sizeof(struct mach_hdr64));
	if (ret != sizeof(struct mach_hdr64)) {
		pr_err("read image failed\n");
		return ret;
	}

	pr_debug("MACH-O magic:           0x%x\n", hdr.magic);
	pr_debug("MACH-O cpu_type:        0x%x\n", hdr.cpu_type);
	pr_debug("MACH-O cpu_sub_type:    0x%x\n", hdr.cpu_sub_type);
	pr_debug("MACH-O file_type:       %d\n", hdr.file_type);
	pr_debug("MACH-O nr_cmds:         %d\n", hdr.nr_cmds);
	pr_debug("MACH-O size_of_cmds:    %d\n", hdr.size_of_cmds);
	pr_debug("MACH-O flags:           0x%x\n", hdr.flags);

	cmds = malloc(BALIGN(hdr.size_of_cmds, PAGE_SIZE));
	if (cmds == NULL)
		return -ENOMEM;

	if (lseek(fd, sizeof(struct mach_hdr64), SEEK_SET) == -1)
		return -EIO;

	ret = read(fd, cmds, hdr.size_of_cmds);
	if (ret <= 0)
		return ret;

	cmd = (struct load_cmd *)cmds;

	for (i = 0; i < hdr.nr_cmds; i++) {
		switch (cmd->cmd) {
		case LC_SEGMENT_64:
			cmd64 = (struct segment_cmd64 *)cmd;
			xnu_new_cmd64(od, cmd64);
			break;
		case LC_UNIXTHREAD:
			od->entry_point = *(uint64_t *)((char*)cmd + 0x110);
			pr_info("xnu entry address is 0x%lx\n", od->entry_point);
			break;
		default:
			break;
		}
		cmd = (struct load_cmd *)((char *)cmd + cmd->cmd_size);
	}

	od->meta_buf = cmds;

	return 0;
}

static void inline xnu_get_macho_highlow(struct xnu_os_data *od,
		uint64_t *l, uint64_t *h)
{
	uint64_t low = ~0, high = 0;
	struct xnu_segment_cmd64 *root = od->root;
	struct segment_cmd64 *cmd;

	while (root) {
		cmd = root->cmd;
		if (cmd->vm_addr < low)
			low = cmd->vm_addr;

		if ((cmd->vm_addr + cmd->vm_size) > high)
			high = cmd->vm_addr + cmd->vm_size;

		root = root->next;
	};

	*l = low;
	*h = high;
}

static void xnu_parse_address_space(struct vm *vm, struct xnu_os_data *od)
{
	struct stat stbuf;
	uint64_t low_addr, high_addr;

	xnu_get_macho_highlow(od, &low_addr, &high_addr);

	od->kernel_load_base = low_addr;
	od->ramdisk_load_base = high_addr;

	if ((vm->rfd <= 0) || (fstat(vm->rfd, &stbuf) != 0) ||
			(!S_ISREG(stbuf.st_mode))) {
		od->dtb_load_base = od->ramdisk_load_base;
		od->ramdisk_size = 0;
		goto _bootarg;
	}

	od->ramdisk_size = stbuf.st_size;
	od->dtb_load_base = od->ramdisk_load_base + stbuf.st_size;
	od->dtb_load_base = (od->dtb_load_base + 0xffffull) & ~0xffffull;

_bootarg:
	if ((vm->dfd <= 0) || (fstat(vm->dfd, &stbuf) != 0) ||
			(!S_ISREG(stbuf.st_mode))) {
		od->bootarg_load_base = od->dtb_load_base;
		od->dtb_size = 0;
		goto out;
	}

	od->dtb_size = stbuf.st_size;
	od->bootarg_load_base = od->dtb_load_base + stbuf.st_size;
	od->bootarg_load_base = (od->bootarg_load_base + 0xffffull) & ~0xffffull;

out:
	od->load_end = od->bootarg_load_base + sizeof(struct xnu_arm64_boot_args);
	od->load_end = (od->load_end + 0xffffull) & ~0xffffull;

	vm->mem_start = XNU_KERNEL_BASE;
	vm->entry = VA2PA(od->entry_point);
	vm->setup_data = VA2PA(od->bootarg_load_base);

	pr_info("xnu kernel_load_base 0x%lx\n", od->kernel_load_base);
	pr_info("xnu ramdisk_load_base 0x%lx\n", od->ramdisk_load_base);
	pr_info("xnu dtb_load_base 0x%lx\n", od->dtb_load_base);
	pr_info("xnu bootarg_load_base 0x%lx\n", od->bootarg_load_base);
}

static int xnu_early_init(struct vm *vm)
{
	int ret;
	struct xnu_os_data *os_data;

	if (vm->kfd <= 0) {
		pr_err("no kernel image opened\n");
		return -EINVAL;
	}

	os_data = malloc(sizeof(struct xnu_os_data));
	if (!os_data)
		return -ENOMEM;

	memset(os_data, 0, sizeof(struct xnu_os_data));

	ret = xnu_parse_kernel_image(vm->kfd, os_data);
	if (ret) {
		free(os_data);
		return ret;
	}

	xnu_parse_address_space(vm, os_data);
	vm->os_data = os_data;

	return 0;
}

struct vm_os os_xnu = {
	.name		= "xnu",
	.type		= OS_TYPE_XNU,
	.early_init	= xnu_early_init,
	.load_image	= xnu_load_image,
	.setup_vm_env	= xnu_setup_env,
	.vm_exit	= xnu_vm_exit,
};
DEFINE_OS(os_xnu);
