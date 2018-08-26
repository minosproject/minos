#ifndef __MINOS_VIRTIO_H__
#define __MINOS_VIRTIO_H__

#include <minos/vdev.h>

struct vm;

struct virtio_device {
	struct vdev vdev;
	int gvm_irq;
};

void *create_virtio_device(struct vm *vm);

#endif
