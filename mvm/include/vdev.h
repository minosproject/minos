#ifndef __MVM_MVM_DEVICE_H__
#define __MVM_MVM_DEVICE_H__

#include <mvm.h>
#include <pthread.h>

#define PDEV_NAME_SIZE		(31)

struct vdev;

struct vdev_ops {
	char *name;
	int (*init)(struct vdev *, char *);
	void (*deinit)(struct vdev *);
	int (*reset)(struct vdev *);
	int (*setup)(struct vdev *, void *data, int os);
	int (*handle_event)(struct vdev *, int,
			unsigned long, unsigned long *);
};

#define VDEV_TYPE_PLATFORM	(0x0)
#define VDEV_TYPE_VIRTIO	(0x1)

struct vdev {
	struct vm *vm;
	int gvm_irq;
	void *iomem;
	void *guest_iomem;
	size_t iomem_size;
	struct vdev_ops *ops;
	void *pdata;
	int dev_type;
	int id;
	char name[PDEV_NAME_SIZE + 1];
	struct list_head list;
	pthread_mutex_t lock;
};

#define DEFINE_VDEV_TYPE(ops)	\
	static void *mvdev_ops_##ops __used __section("vdev_ops") = &ops

extern void * __start_vdev_ops;
extern void *__stop_vdev_ops;

int create_vdev(struct vm *vm, char *class, char *args);
void *vdev_map_iomem(void *iomem, size_t size);
void vdev_unmap_iomem(void *iomem, size_t size);
void vdev_setup_env(struct vm *vm, void *data, int os_type);
void vdev_send_irq(struct vdev *vdev);
void release_vdev(struct vdev *vdev);

static void inline vdev_set_pdata(struct vdev *vdev, void *data)
{
	if (vdev)
		vdev->pdata = data;
}

static inline void *vdev_get_pdata(struct vdev *vdev)
{
	return (vdev ? vdev->pdata : NULL);
}

#endif
