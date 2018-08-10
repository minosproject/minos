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
#include <mvm_device.h>

LIST_HEAD(mdev_list);

void mdev_send_irq(struct mvm_device *mdev)
{
	if (!mdev->gvm_irq)
		return;

	send_virq_to_vm(mdev->vm, mdev->gvm_irq);
}

static struct dev_ops *get_dev_ops(char *class)
{
	struct dev_ops *start = (struct dev_ops *)&__start_mdev_ops;
	struct dev_ops *end = (struct dev_ops *)&__stop_mdev_ops;

	for (; start < end; start++) {
		if (strcmp(start->name, class) == 0)
			return start;
	}

	return NULL;
}

static struct mvm_device *
alloc_and_init_mvm_device(struct vm *vm, char *class, char *args)
{
	int len, ret;
	struct mvm_device *pdev;
	struct dev_ops *plat_ops = NULL;
	char buf[32];

	plat_ops = get_dev_ops(class);
	if (!plat_ops)
		return NULL;

	pdev = malloc(sizeof(struct mvm_device));
	if (!pdev)
		return NULL;

	memset(pdev, 0, sizeof(struct mvm_device));
	pdev->ops = plat_ops;
	pdev->vm = vm;

	memset(buf, 0, 32);
	len = strlen(class);
	if (len > PDEV_NAME_SIZE - 4)
		strncpy(buf, class, PDEV_NAME_SIZE - 4);
	else
		strcpy(buf, class);
	sprintf(pdev->name, "dev-%s", buf);

	ret = plat_ops->dev_init(pdev, args);
	if (ret) {
		free(pdev);
		pdev = NULL;
	}

	return pdev;
}

static int register_mvm_device(struct mvm_device *pdev)
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
			printf("register %s failed\n", pdev->name);
			return ret;
		}
	}

	return 0;
}

void release_mvm_device(struct mvm_device *mdev)
{
	if (!mdev)
		return;

	mdev->ops->dev_deinit(mdev);
	free(mdev);
}

int create_mvm_device(struct vm *vm, char *class, char *args)
{
	int ret = 0;
	struct mvm_device *mdev;

	mdev = alloc_and_init_mvm_device(vm, class, args);
	if (!mdev)
		return -ENOMEM;

	ret = register_mvm_device(mdev);
	if (ret)
		goto release_mdev;

	list_add_tail(&mdev_list, &mdev->list);

	return 0;

release_mdev:
	release_mvm_device(mdev);
	return ret;
}
