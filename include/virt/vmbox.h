#ifndef __MINOS_VMBOX_H__
#define __MINOS_VMBOX_H__

#include <minos/device_id.h>
#include <virt/vdev.h>

#define VMBOX_VRING_ALGIN_SIZE	8

#define VMBOX_DEV_STAT_ONLINE	0x1

struct vmbox_device {
	int devid;
	int vmbox_id;
	int is_backend;
	int state;
	struct vm *vm;
	uint32_t vring_virq;
	uint32_t ipc_virq;
	void *dev_reg;
	unsigned long iomem;
	size_t iomem_size;
	struct vmbox_controller *vc;
	struct vmbox_device *bro;
};

/*
 * owner : owner[0] is the server vm and the
 *         owner[1] is the client vm
 * devid : device id and vendor id of this device
 * vmbox_reg_base : the iobase of this controller
 */
struct vmbox {
	int id;
	char name[32];
	uint32_t owner[2];
	uint32_t devid[2];
	void *shmem;
	size_t shmem_size;
	uint32_t vqs;
	uint32_t vring_num;
	uint32_t vring_size;
	unsigned long flags;
	struct vmbox_device *devices[2];
};

#define VMBOX_F_PLATFORM_DEV	(1 << 0)

struct vmbox_hook_ops {
	int (*vmbox_init)(struct vmbox *vmbox);
	int (*vmbox_be_init)(struct vm *vm, struct vmbox *vmbox,
			struct vmbox_device *vdev);
	int (*vmbox_fe_init)(struct vm *vm, struct vmbox *vmbox,
			struct vmbox_device *vdev);
};

struct vmbox_controller {
	void *pa;
	void *va;
	struct vm *vm;
	int dev_cnt;
	int status;
	uint32_t virq;
	struct list_head list;
	struct vdev vdev;
	struct vmbox_device *devices[32];
};

#define vdev_to_vmbox_con(dev) \
	container_of(dev, struct vmbox_controller, vdev)

/*
 * below are the defination of the vmbox controller
 * and device register map, each controller will have
 * a 4K IO memory space, 0x0-0xff is for controller
 * itself, and 0x100 - 0xfff is for the vmbox devices
 */
#define VMBOX_DEVICE_MAGIC		0xabcdef00

#define VMBOX_CON_DEV_STAT		0x00	/* RO state of each device */
#define VMBOX_CON_ONLINE		0x04	/* WO to inform the controller is online */
#define VMBOX_CON_INT_STATUS		0x08	/* RO virq will send by hypervisor */

#define VMBOX_CON_INT_TYPE_DEV_ONLINE	(1 << 0)

#define VMBOX_CON_DEV_BASE 		0x100
#define VMBOX_CON_DEV_SIZE 		0x40

#define VMBOX_DEV_ID			0x00	/* RO */
#define VMBOX_DEV_VQS			0x04	/* RO */
#define VMBOX_DEV_VRING_NUM 		0X08	/* RO */
#define VMBOX_DEV_VRING_SIZE 		0x0c	/* RO */
#define VMBOX_DEV_VRING_BASE_HI		0x10	/* RO */
#define VMBOX_DEV_VRING_BASE_LOW	0x14	/* RO */
#define VMBOX_DEV_MEM_SIZE		0x18
#define VMBOX_DEV_DEVICE_ID		0x1c	/* RO */
#define VMBOX_DEV_VENDOR_ID		0x20	/* RO */
#define VMBOX_DEV_VRING_IRQ		0x24	/* RO */
#define VMBOX_DEV_IPC_IRQ		0x28	/* RO */
#define VMBOX_DEV_VRING_EVENT		0x2c	/* WO trigger a vring event */
#define VMBOX_DEV_IPC_EVENT		0x30	/* WO trigger a config event */
#define VMBOX_DEV_IPC_TYPE		0x34	/* RW */
#define VMBOX_DEV_IPC_ACK		0x38	/* event ack */
#define VMBOX_DEV_VDEV_ONLINE		0x3C	/* only for client device */

#define VMBOX_DEV_EVENT_ONLINE		0x1
#define VMBOX_DEV_EVENT_OFFLINE		0x2
#define VMBOX_DEV_EVENT_OPENED		0x3
#define VMBOX_DEV_EVENT_CLOSED		0x4
#define VMBOX_DEV_EVENT_USER_BASE	0x1000

int register_vmbox_hook(char *name, struct vmbox_hook_ops *ops);
int of_create_vmbox(struct device_node *node);
struct vmbox_controller *vmbox_get_controller(struct vm *vm);
int vmbox_register_platdev(struct vmbox_device *vdev,
		void *dtb, char *type);

#endif
