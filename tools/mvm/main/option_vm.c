/*
 * BSD 3-Clause License
 *
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <minos/vm.h>
#include <minos/option.h>
#include <common/hypervisor.h>

static int setup_vm_mem_size(char *arg, char *sub_arg, void *data)
{
	struct vm *vm = (struct vm *)data;
	uint32_t size;

	if (sscanf((const char *)arg, "%uM", &size) <= 0)
		return -EINVAL;

	vm->mem_size = size * 1024 * 1024;
	vm->mem_size = (vm->mem_size + 0x1fffff) & ~(0x1fffff);

	return 0;
}

static int setup_vm_name(char *arg, char *sub_arg, void *data)
{
	struct vm *vm = (struct vm *)data;

	strncpy(vm->name, arg, sizeof(vm->name) - 1);
	return 0;
}

static int setup_vm_os_type(char *arg, char *sub_arg, void *data)
{
	struct vm *vm = (struct vm *)data;

	vm->os = get_vm_os(arg);
	if (!vm->os)
		return -EINVAL;

	return 0;
}

static int setup_vm_vcpus(char *arg, char *sub_arg, void *data)
{
	struct vm *vm = (struct vm *)data;

	if (!arg)
		return -EINVAL;

	vm->nr_vcpus = atoi(arg);
	if (vm->nr_vcpus <= 0)
		return -EINVAL;

	return 0;
}

static int setup_vm_bootimage(char *arg, char *sub_arg, void *data)
{
	struct vm *vm = (struct vm *)data;

	if (arg == NULL)
		return -EINVAL;

	vm->image_fd = open(arg, O_RDONLY | O_NONBLOCK);
	if (vm->image_fd <= 0) {
		pr_err("failed to open bootimage %s\n", arg);
		return -EIO;
	}

	vm->flags &= ~VM_FLAGS_NO_BOOTIMAGE;

	return 0;
}

static int setup_vm_kernel_image(char *arg, char *sub_arg, void *data)
{
	struct vm *vm = (struct vm *)data;

	vm->kfd = open(arg, O_RDONLY | O_NONBLOCK);
	if (vm->kfd <= 0)
		pr_err("failed to open kernel image %s\n", arg);

	return 0;
}

static int setup_vm_dtb_image(char *arg, char *sub_arg, void *data)
{
	struct vm *vm = (struct vm *)data;

	vm->dfd = open(arg, O_RDONLY | O_NONBLOCK);
	if (vm->dfd <= 0)
		pr_err("failed to open dtb image %s\n", arg);

	return 0;
}

static int setup_vm_ramdisk_image(char *arg, char *sub_arg, void *data)
{
	struct vm *vm = (struct vm *)data;

	vm->rfd = open(arg, O_RDONLY | O_NONBLOCK);
	if (vm->rfd <= 0)
		pr_err("failed to open ramdisk image %s\n", arg);
	else
		vm->flags &= ~VM_FLAGS_NO_RAMDISK;

	return 0;
}

static int setup_vm_ramdisk_flag(char *arg, char *sub_arg, void *data)
{
	struct vm *vm = (struct vm *)data;

	vm->flags |= VM_FLAGS_NO_RAMDISK;
	return 0;
}

static int setup_vm_entry(char *arg, char *sub_arg, void *data)
{
	struct vm *vm = (struct vm *)data;

	sscanf((const char *)arg, "0x%"PRIx64, &vm->entry);
	return 0;
}

static int setup_vm_setup_addr(char *arg, char *sub_arg, void *data)
{
	struct vm *vm = (struct vm *)data;

	sscanf((const char *)arg, "0x%"PRIx64, &vm->setup_data);
	return 0;
}

static int setup_vm_mem_base(char *arg, char *sub_arg, void *data)
{
	struct vm *vm = (struct vm *)data;

	sscanf((const char *)arg, "0x%"PRIx64, &vm->mem_start);
	vm->mem_start = BALIGN(vm->mem_start, MEM_BLOCK_SIZE);

	return 0;
}

static int setup_vm_type(char *arg, char *sub_arg, void *data)
{
	struct vm *vm = (struct vm *)data;

	pr_info("os is 32bit\n");
	vm->flags |= VM_FLAGS_32BIT;

	return 0;
}

static int setup_vm_cmdline(char *arg, char *sub_arg, void *data)
{
	struct vm *vm = (struct vm *)data;

	vm->cmdline = arg;
	return 0;
}

static int setup_vm_gic(char *arg, char *sub_arg, void *data)
{
	struct vm *vm = (struct vm *)data;

	if (!arg)
		return 0;

	if (strcmp(arg, "gicv3") == 0)
		vm->gic_type = GIC_TYPE_GICV3;
	else if (strcmp(arg, "gicv4") == 0)
		vm->gic_type = GIC_TYPE_GICV4;

	return 0;
}

static int setup_vm_wfi(char *arg, char *sub_arg, void *data)
{
	struct vm *vm = data;

	pr_info("disable WFI trap\n");
	vm->flags |= VM_FLAGS_NATIVE_WFI;

	return 0;
}

DEFINE_OPTION_VM(mem_size, "memory", 1, setup_vm_mem_size);
DEFINE_OPTION_VM(name, "vm_name", 0, setup_vm_name);
DEFINE_OPTION_VM(os_type, "vm_os", 1, setup_vm_os_type);
DEFINE_OPTION_VM(vcpus, "vcpus", 1, setup_vm_vcpus);
DEFINE_OPTION_VM(bootimage, "bootimage", 0, setup_vm_bootimage);
DEFINE_OPTION_VM(kfd, "kernel_image", 0, setup_vm_kernel_image);
DEFINE_OPTION_VM(dfd, "dtb_image", 0, setup_vm_dtb_image);
DEFINE_OPTION_VM(rfd, "ramdisk_image", 0, setup_vm_ramdisk_image);
DEFINE_OPTION_VM(rf, "no-ramdisk", 0, setup_vm_ramdisk_flag);
DEFINE_OPTION_VM(entry, "entry", 0, setup_vm_entry);
DEFINE_OPTION_VM(setup_data, "setup", 0, setup_vm_setup_addr);
DEFINE_OPTION_VM(setup_mem_base,
		"iomem_base", 0, setup_vm_mem_base);
DEFINE_OPTION_VM(type, "os-32bit", 0, setup_vm_type);
DEFINE_OPTION_VM(cmdline, "cmdline", 0, setup_vm_cmdline);
DEFINE_OPTION_VM(gic, "gic", 0, setup_vm_gic);
DEFINE_OPTION_VM(wfi, "native_wfi", 0, setup_vm_wfi);
