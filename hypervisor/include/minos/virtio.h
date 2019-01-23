#ifndef __MINOS_VIRTIO_H__
#define __MINOS_VIRTIO_H__

#include <minos/vdev.h>

struct vm;

struct virtio_device {
	struct vdev vdev;
};

int virtio_mmio_init(struct vm *vm, size_t size,
		unsigned long *gbase, unsigned long *hbase);
int virtio_mmio_deinit(struct vm *vm);

#endif
