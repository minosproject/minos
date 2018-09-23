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

#include <mvm.h>
#include <sys/ioctl.h>
#include <vdev.h>
#include <list.h>
#include <sys/mman.h>
#include <libfdt/libfdt.h>

void *vdev_map_iomem(void *base, size_t size)
{
	return hvm_map_iomem(base, size);
}

void vdev_unmap_iomem(void *base, size_t size)
{
	if (munmap(base, size))
		pr_err("unmap io memory 0x%p failed\n", base);
}

void vdev_send_irq(struct vdev *vdev)
{
	if (!vdev->gvm_irq)
		return;

	send_virq_to_vm(vdev->gvm_irq);
}

static struct vdev_ops *get_vdev_ops(char *class)
{
	struct vdev_ops *ops;
	struct vdev_ops **start = (struct vdev_ops **)&__start_vdev_ops;
	struct vdev_ops **end = (struct vdev_ops **)&__stop_vdev_ops;

	for (; start < end; start++) {
		ops = *start;
		if (strcmp(ops->name, class) == 0)
			return ops;
	}

	return NULL;
}

static struct vdev *
alloc_and_init_vdev(struct vm *vm, char *class, char *args)
{
	int len, ret;
	static int vdev_id = 0;
	struct vdev *pdev;
	struct vdev_ops *plat_ops = NULL;
	char buf[32];

	plat_ops = get_vdev_ops(class);
	if (!plat_ops) {
		pr_err("can not find such vdev %s\n", class);
		return NULL;
	}

	pdev = malloc(sizeof(struct vdev));
	if (!pdev)
		return NULL;

	memset(pdev, 0, sizeof(struct vdev));
	pdev->ops = plat_ops;
	pdev->vm = vm;

	memset(buf, 0, 32);
	len = strlen(class);
	if (len > PDEV_NAME_SIZE - 2)
		strncpy(buf, class, PDEV_NAME_SIZE - 2);
	else
		strcpy(buf, class);
	sprintf(pdev->name, "%s%2d", buf, vdev_id);
	vdev_id++;

	ret = plat_ops->init(pdev, args);
	if (ret) {
		free(pdev);
		pdev = NULL;
	}

	return pdev;
}

void release_vdev(struct vdev *vdev)
{
	if (!vdev)
		return;

	vdev->ops->deinit(vdev);
	free(vdev);
}

int create_vdev(struct vm *vm, char *class, char *args)
{
	struct vdev *vdev;

	vdev = alloc_and_init_vdev(vm, class, args);
	if (!vdev)
		return -ENOMEM;

	list_add_tail(&vm->vdev_list, &vdev->list);

	return 0;
}

static int dtb_add_platform(struct vdev *vdev, char *dtb, int offset)
{
	return 0;
}

static int dtb_add_virtio(struct vdev *vdev, char *dtb, int offset)
{
	int node;
	char buf[64];
	uint32_t args[3];
	uint32_t addr;

	memset(buf, 0, 64);
	addr = (uint32_t)vdev->guest_iomem - 0x40000000;
	sprintf(buf, "%s@%x", vdev->name, addr);
	node = fdt_add_subnode(dtb, offset, buf);
	if (node < 0) {
		pr_err("add %s failed\n", vdev->name);
		return -EINVAL;
	}

	fdt_setprop(dtb, node, "compatible", "virtio,mmio", 12);

	/* setup the reg value */
	args[0] = cpu_to_fdt32(addr);
	args[1] = cpu_to_fdt32(PAGE_SIZE);
	fdt_setprop(dtb, node, "reg", (void *)args,
			2 * sizeof(uint32_t));

	/* setup the interrupt */
	if (vdev->gvm_irq) {
		args[0] = cpu_to_fdt32(0x0);
		args[1] = cpu_to_fdt32(vdev->gvm_irq - 32);
		args[2] = cpu_to_fdt32(4);
		fdt_setprop(dtb, node, "interrupts", (void *)args,
				3 * sizeof(uint32_t));
	}

	pr_info("add vdev success addr-0x%lx virq-%d\n",
			vdev->guest_iomem, vdev->gvm_irq);
	return 0;
}

static void vdev_setup_dtb(struct vm *vm, char *dtb)
{
	struct vdev *vdev;
	int offset;

	offset = fdt_path_offset(dtb, "/smb/motherboard/vdev");
	if (offset < 0) {
		pr_err("set up vdev failed no vdev node\n");
		return;
	}

	list_for_each_entry(vdev, &vm->vdev_list, list) {
		switch (vdev->dev_type) {
		case VDEV_TYPE_PLATFORM:
			dtb_add_platform(vdev, dtb, offset);
			break;
		case VDEV_TYPE_VIRTIO:
			dtb_add_virtio(vdev, dtb, offset);
			break;
		default:
			pr_err("unsupported device type now\n");
			break;
		}
	}
}

void vdev_setup_env(struct vm *vm, char *data, int os_type)
{
	switch (os_type) {
	case OS_TYPE_LINUX:
		vdev_setup_dtb(vm, data);
		break;
	default:
		break;
	}
}
