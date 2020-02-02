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

#include <minos/vm.h>
#include <sys/ioctl.h>
#include <minos/vdev.h>
#include <minos/list.h>
#include <sys/mman.h>
#include <libfdt/libfdt.h>
#include <common/gvm.h>
#include <minos/option.h>

static int vdev_irq_base = GVM_IRQ_BASE;
static int vdev_irq_count = GVM_IRQ_COUNT;
static int vdev_id;

void *vdev_map_iomem(unsigned long base, size_t size)
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

#ifdef __CLANG__
extern struct vdev_ops virtio_console_ops;
extern struct vdev_ops virtio_net_ops;
extern struct vdev_ops virtio_blk_ops;
extern struct vdev_ops s3c_uart_ops;

static struct vdev_ops *vdev_opses[] = {
	&virtio_console_ops,
	&virtio_net_ops,
	&virtio_blk_ops,
	&s3c_uart_ops,
	NULL,
};

static struct vdev_ops *get_vdev_ops(char *class)
{
	int i, j;
	struct vdev_ops *ops;

	for (i = 0; ; i++) {
		ops = vdev_opses[i];

		if (ops == NULL)
			break;

		if (strcmp(ops->name, class) == 0)
			return ops;
	}

	return NULL;
}
#else
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
#endif

void release_vdev(struct vdev *vdev)
{
	if (!vdev)
		return;

	vdev->ops->deinit(vdev);
	free(vdev);
}

static int __vdev_request_virq(struct vm *vm, int base, int nr)
{
	unsigned long arg[2];

	/* request the virq from the hypervisor */
	arg[0] = base;
	arg[1] = nr;
	return ioctl(vm->vm_fd, IOCTL_REQUEST_VIRQ, arg);
}

static int __vdev_alloc_and_request_irq(struct vm *vm, int nr, int request)
{
	int base = 0, ret = 0;

	if (vdev_irq_count < nr)
		return 0;

	/* try to request the virq */
	if (request)
		ret = __vdev_request_virq(vm, vdev_irq_base, nr);

	if (!ret) {
		base = vdev_irq_base;
		vdev_irq_base += nr;
		vdev_irq_count -= nr;
	}

	return base;
}

int vdev_alloc_irq(struct vm *vm, int nr)
{
	return __vdev_alloc_and_request_irq(vm, nr, 0);
}

int vdev_alloc_and_request_irq(struct vm *vm, int nr)
{
	return __vdev_alloc_and_request_irq(vm, nr, 1);
}

static int dtb_add_virtio(struct vdev *vdev, void *dtb)
{
	int node, offset;
	char buf[64];
	uint32_t args[3];
	uint64_t addr;

	offset = fdt_path_offset(dtb, "/smb");
	if (offset < 0) {
		pr_err("set up vdev failed no vdev node\n");
		return -ENOENT;
	}

	memset(buf, 0, 64);
	addr = vdev->guest_iomem;
	sprintf(buf, "%s@%"PRIx64, vdev->name, addr);
	node = fdt_add_subnode(dtb, offset, buf);
	if (node < 0) {
		pr_err("add %s failed\n", vdev->name);
		return -EINVAL;
	}

	fdt_setprop(dtb, node, "compatible", "virtio,mmio", 12);
	fdt_setprop(dtb, node, "virtual_device", "", 1);

	/* setup the reg value */
	args[0] = cpu_to_fdt32((unsigned long)addr);
	args[1] = cpu_to_fdt32(vdev->iomem_size);
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

	return 0;
}

static void vdev_setup_dtb(struct vm *vm, void *dtb)
{
	struct vdev *vdev;

	list_for_each_entry(vdev, &vm->vdev_list, list) {
		switch (vdev->dev_type) {
		case VDEV_TYPE_PLATFORM:
			if (vdev->ops->setup)
				vdev->ops->setup(vdev, dtb, OS_TYPE_LINUX);
			break;
		case VDEV_TYPE_VIRTIO:
			dtb_add_virtio(vdev, dtb);
			if (vdev->ops->setup)
				vdev->ops->setup(vdev, dtb, OS_TYPE_LINUX);
			break;
		default:
			pr_err("unsupported device type now\n");
			break;
		}
	}
}

void vdev_setup_env(struct vm *vm, void *data, int os_type)
{
	switch (os_type) {
	case OS_TYPE_LINUX:
		vdev_setup_dtb(vm, data);
		break;
	default:
		break;
	}
}

int create_vdev(struct vm *vm, char *name, char *args)
{
	int ret;
	struct vdev *vdev = NULL;
	struct vdev_ops *vdev_ops;

	vdev_ops = get_vdev_ops(name);
	if (!vdev_ops)
		return -ENOENT;

	vdev = calloc(1, sizeof(struct vdev));
	if (!vdev)
		return -ENOMEM;

	vdev->ops = vdev_ops;
	vdev->vm = vm;
	vdev->id = vdev_id++;
	vdev->dev_type = VDEV_TYPE_PLATFORM;
	pthread_mutex_init(&vdev->lock, NULL);

	strncpy(vdev->name, name, sizeof(vdev->name) - 1);

	if (vdev_ops->init)
		ret = vdev_ops->init(vdev, args);
	if (ret)
		goto out;

	list_add_tail(&vm->vdev_list, &vdev->list);

	return 0;
out:
	free(vdev);
	return ret;
}

static int setup_vm_vdev(char *arg, char *sub_arg, void *data)
{
	return create_vdev((struct vm *)data, arg, sub_arg);
}
DEFINE_OPTION_VDEV(vm_vdev, "device", 0, setup_vm_vdev);
