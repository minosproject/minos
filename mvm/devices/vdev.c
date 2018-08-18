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

#include <mvm.h>
#include <sys/ioctl.h>
#include <vdev.h>
#include <list.h>
#include <sys/mman.h>
#include <libfdt/libfdt.h>

void *vdev_map_iomem(void *base, size_t size)
{
	void *iomem;
	int fd = open("/dev/mvm/mvm0", O_RDWR);

	if (fd < 0) {
		printf("* error - open /dev/mvm/mvm0 failed\n");
		return (void *)-1;
	}

	iomem = mmap(NULL, size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, (unsigned long)base);
	close(fd);

	return iomem;
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
		printf("* error - can not find such vdev %s\n", class);
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

	ret = plat_ops->dev_init(pdev, args);
	if (ret) {
		free(pdev);
		pdev = NULL;
	}

	return pdev;
}

static int register_vdev(struct vdev *pdev)
{
	int ret;
	unsigned long args[2];

	if (!pdev)
		return -EINVAL;

	/* register irq and the event handler */
	if (pdev->hvm_irq) {
		args[0] = pdev->hvm_irq | ((unsigned long)getpid() << 32);
		args[1] = (unsigned long)pdev;

		ret = ioctl(pdev->vm->vm_fd, IOCTL_REGISTER_MDEV, args);
		if (ret) {
			printf("* error - register event for %s failed\n",
					pdev->name);
			return ret;
		}
	}

	return 0;
}

void release_vdev(struct vdev *vdev)
{
	if (!vdev)
		return;

	vdev->ops->dev_deinit(vdev);
	free(vdev);
}

int create_vdev(struct vm *vm, char *class, char *args)
{
	int ret = 0;
	struct vdev *vdev;

	vdev = alloc_and_init_vdev(vm, class, args);
	if (!vdev)
		return -ENOMEM;

	ret = register_vdev(vdev);
	if (ret)
		goto release_vdev;

	list_add_tail(&vm->vdev_list, &vdev->list);

	return 0;

release_vdev:
	release_vdev(vdev);
	return ret;
}

static void vdev_setup_dtb(struct vm *vm, char *dtb)
{
	struct vdev *vdev;
	int offset, node;
	char buf[64];
	uint32_t args[3];
	uint32_t addr;

	offset = fdt_path_offset(dtb, "/smb/motherboard/vdev");
	if (offset < 0) {
		printf("* error - set up vdev failed no vdev node\n");
		return;
	}

	list_for_each_entry(vdev, &vm->vdev_list, list) {
		memset(buf, 0, 64);
		addr = (uint32_t)vdev->guest_iomem - 0x40000000;
		sprintf(buf, "%s@%x", vdev->name, addr);
		node = fdt_add_subnode(dtb, offset, buf);
		if (node < 0) {
			printf("* error - add %s failed\n", vdev->name);
			continue;
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

		pr_info("add vdev success 0x%x %d\n", addr, vdev->gvm_irq);
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
