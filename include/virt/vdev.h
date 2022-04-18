#ifndef __MINOS_VDEV_H__
#define __MINOS_VDEV_H__

#include <minos/types.h>
#include <minos/list.h>
#include <asm/arch.h>
#include <virt/virq.h>
#include <minos/device_id.h>

#define VDEV_NAME_SIZE	(15)

typedef void *(*vdev_init_t)(struct vm *vm, struct device_node *node);

struct vdev {
	char name[VDEV_NAME_SIZE + 1];
	int host;
	struct vm *vm;
	struct vmm_area *gvm_area;
	struct list_head list;

	int (*read)(struct vdev *, gp_regs *, int,
			unsigned long, unsigned long *);
	int (*write)(struct vdev *, gp_regs *, int,
			unsigned long, unsigned long *);
	void (*deinit)(struct vdev *vdev);
	void (*reset)(struct vdev *vdev);
	int (*suspend)(struct vdev *vdev);
	int (*resume)(struct vdev *vdev);
};

struct vdev *create_host_vdev(struct vm *vm, const char *name);

void vdev_release(struct vdev *vdev);

void host_vdev_init(struct vm *vm, struct vdev *vdev, const char *name);

int vdev_mmio_emulation(gp_regs *regs, int write,
		unsigned long address, unsigned long *value);

int vdev_add_iomem_range(struct vdev *vdev,
		unsigned long base, size_t size);

struct vmm_area *vdev_alloc_iomem_range(struct vdev *vdev,
		size_t size, int flags);

struct vmm_area *vdev_get_vmm_area(struct vdev *vdev, int idx);

void vdev_add(struct vdev *vdev);

static int inline vdev_notify_gvm(struct vdev *vdev, uint32_t irq)
{
	return send_virq_to_vm(vdev->vm, irq);
}

static int inline vdev_notify_hvm(struct vdev *vdev, uint32_t irq)
{
	return send_virq_to_vm(get_host_vm(), irq);
}

#endif
