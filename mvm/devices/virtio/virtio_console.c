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
#include <virtio.h>
#include <virtio_mmio.h>
#include <vdev.h>
#include <io.h>

static int virtio_console_init(struct vdev *vdev, char *class)
{
	int ret = 0;
	void *iomem;

	iomem = hv_create_virtio_device(vdev->vm);
	if (!iomem) {
		printf("hypervisor create device failed\n");
		return 0;
	}

	printv("get new virtio dev at 0x%lx\n", (unsigned long)iomem);

	ret = virtio_device_init(vdev, iomem, VIRTIO_TYPE_CONSOLE);
	if (ret) {
		printf("failed to init vdev %d\n", ret);
		return ret;
	}

	return 0;
}

static int virtio_console_deinit(struct vdev *vdev)
{
	return 0;
}

static int virtio_console_event(struct vdev *vdev)
{
	printf("virtio console event\n");
	return 0;
}

struct vdev_ops virtio_console_ops = {
	.name 		= "virtio_console",
	.dev_init	= virtio_console_init,
	.dev_deinit	= virtio_console_deinit,
	.handle_event	= virtio_console_event,
};

DEFINE_MDEV_TYPE(virtio_console_ops);
