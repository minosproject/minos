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

#define VIRTIO_CONSOLE_F_SIZE		(0)
#define VIRTIO_CONSOLE_F_MULTIPORT	(1)
#define VIRTIO_CONSOLE_F_EMERG_WRITE	(2)

#define VRITIO_CONSOLE_RECQ_IDX		(0)
#define VRITIO_CONSOLE_TRSQ_IDX		(1)

#define VIRTIO_CONSOLE_DEVICE_READY     0
#define VIRTIO_CONSOLE_PORT_ADD         1
#define VIRTIO_CONSOLE_PORT_REMOVE      2
#define VIRTIO_CONSOLE_PORT_READY       3
#define VIRTIO_CONSOLE_CONSOLE_PORT     4
#define VIRTIO_CONSOLE_RESIZE           5
#define VIRTIO_CONSOLE_PORT_OPEN        6
#define VIRTIO_CONSOLE_PORT_NAME        7

#define VIRTIO_CONSOLE_RING_SIZE	(64)

struct virtio_console_config {
	uint16_t cols;
	uint16_t rows;
	uint32_t max_nr_ports;
	uint32_t emerg_wr;
};

struct virtio_console_control {
	uint32_t id;          /* Port number */
	uint16_t event;       /* The kind of control event (see below) */
	uint16_t value;       /* Extra information for the key */
};

struct virtio_console {
	struct virtio_device virtio_dev;
};

static int vcon_rx_handler(struct virtio_device *dev,
		struct virt_queue *queue)
{
	return 0;
}

static int vcon_tx_handler(struct virtio_device *dev,
		struct virt_queue *queue)
{
	return 0;
}

static int vcon_init_vq(struct virtio_device *dev,
		struct virt_queue *queue)
{
	if (queue->queue_index == 0) {
		queue->callback = vcon_rx_handler;
	} else if (queue->queue_index == 1) {
		queue->callback = vcon_tx_handler;
	} else
		pr_err("only support signal port vcon\n");

	return 0;
}

struct virtio_ops vcon_virtio_ops = {
	.vq_init = vcon_init_vq,
};

static int virtio_console_init(struct vdev *vdev, char *class)
{
	int ret = 0;
	struct virtio_console *vcon;

	vcon = (struct virtio_console *)malloc(sizeof(*vcon));
	if (!vcon)
		return -ENOMEM;

	/* do not support mutilport only request 2 virt queue */
	memset(vcon, 0, sizeof(struct virtio_console));
	ret = virtio_device_init(&vcon->virtio_dev, vdev,
			VIRTIO_TYPE_CONSOLE, 2, VIRTIO_CONSOLE_RING_SIZE);
	if (ret) {
		pr_err("failed to init vdev %d\n", ret);
		goto free_vcon;
	}

	vdev_set_pdata(vdev, vcon);
	vcon->virtio_dev.ops = &vcon_virtio_ops;

	/* set the feature of the virtio dev */
	virtio_set_feature(&vcon->virtio_dev, VIRTIO_CONSOLE_F_SIZE);
	virtio_set_feature(&vcon->virtio_dev, VIRTIO_F_VERSION_1);

	return 0;

free_vcon:
	free(vcon);
	return ret;
}

static int virtio_console_deinit(struct vdev *vdev)
{
	return 0;
}

static int virtio_console_event(struct vdev *vdev)
{
	struct virtio_console *vcon;

	if (!vdev)
		return -EINVAL;

	vcon = (struct virtio_console *)vdev_get_pdata(vdev);
	if (!vcon)
		return -EINVAL;

	return virtio_handle_event(&vcon->virtio_dev);
}

struct vdev_ops virtio_console_ops = {
	.name 		= "virtio_console",
	.dev_init	= virtio_console_init,
	.dev_deinit	= virtio_console_deinit,
	.handle_event	= virtio_console_event,
};

DEFINE_MDEV_TYPE(virtio_console_ops);
