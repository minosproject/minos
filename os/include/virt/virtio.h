#ifndef __MINOS_VIRTIO_H__
#define __MINOS_VIRTIO_H__

#include <virt/vdev.h>

struct vm;

struct virtio_device {
	struct vdev vdev;
};

int virtio_mmio_init(struct vm *vm, unsigned long gbase,
		size_t size, unsigned long *hbase);

#endif
