#ifndef __LINUX_VMBOX_BUS_H__
#define __LINUX_VMBOX_BUS_H__

#include <linux/device.h>
#include "vmbox_virtq.h"

#define VMBOX_ANY_DEV_ID	0xffff
#define VMBOX_ANY_VENDOR_ID	0xffff

enum {
	VMBOX_DEV_STAT_OFFLINE = 0,
	VMBOX_DEV_STAT_ONLINE,
	VMBOX_DEV_STAT_OPENED,
	VMBOX_DEV_STAT_CLOSED,
};

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
#define VMBOX_DEV_VDEV_ONLINE		0x34	/* only for client device */
#define VMBOX_DEV_IPC_TYPE		0x38	/* the event type, read and clear */

#define VMBOX_DEV_IPC_COUNT		32

#define VMBOX_IPC_STATE_CHANGE		0x0
#define VMBOX_IPC_USER_BASE		0x8
#define VMBOX_IPC_USER_EVENT(n)		(n + VMBOX_IPC_USER_BASE)

struct vmbox_ipc_entry {
	volatile uint32_t state;
	char buf[0];
};

#define VMBOX_IPC_PER_ENTRY_SZIE	0x80
#define VMBOX_IPC_ALL_ENTRY_SIZE	0x100		// 0x80 * 2

struct vmbox_device_id {
	u32 device;
	u32 vendor;
};

#define VMBOX_F_NO_VIRTQ		(1 << 0)
#define VMBOX_F_DEV_BACKEND		(1 << 1)
#define VMBOX_F_VRING_IRQ_MANUAL	(1 << 2)

struct vmbox_device {
	struct device dev;
	int index;
	struct vmbox_device_id id;

	/*
	 * iomem     - the device regs in vmbox controller space
	 * vring     - the memory for data transfer and ipc event
	 * ipc_in    - the ipc event from otherside
	 * ipc_out   - the ipc event to otherside
	 * data_base - the memory that driver can use
	 */
	void *iomem;

	size_t vring_mem_size;
	void *vring_pa;
	void *vring_va;
	void *data_base;

	struct vmbox_ipc_entry *ipc_in;
	struct vmbox_ipc_entry *ipc_out;

	struct workqueue_struct *workqueue;
	struct work_struct ws;

	int nr_vqs;
	int vring_num;
	int vring_size;
	int vring_irq;
	int event_irq;
	spinlock_t lock;
	unsigned long flags;
	struct vmbox_virtqueue **vqs;
};

#define vmbox_device_vring_start(vdev)		vdev->data_base
#define vmbox_device_data_start(vdev)		vdev->data_base

#define vmbox_device_state(vdev)		vdev->ipc_out->state
#define vmbox_device_otherside_state(vdev)	vdev->ipc_in->state
#define vmbox_device_otherside_open(vdev)	\
	(vdev->ipc_in->state == VMBOX_DEV_STAT_OPENED)

struct vmbox_driver {
	struct device_driver driver;
	const struct vmbox_device_id *id_table;
	int (*probe)(struct vmbox_device *dev);
	void (*remove)(struct vmbox_device *dev);
	void (*setup_vq)(struct vmbox_device *dev);
	int (*otherside_evt_handler)(struct vmbox_device *dev, uint32_t event);
	int (*otherside_state_change)(struct vmbox_device *dev, int state);
};

#define vmbox_get_drvdata(d)	dev_get_drvdata(&(d)->dev)
#define vmbox_set_drvdata(d,p)	dev_set_drvdata(&(d)->dev, p)

static inline struct vmbox_device *to_vmbox_device(struct device *dev)
{
	return container_of(dev, struct vmbox_device, dev);
}

static inline struct vmbox_driver *to_vmbox_driver(struct device_driver *drv)
{
	return container_of(drv, struct vmbox_driver, driver);
}

int vmbox_device_online(struct vmbox_device *vdev);

int vmbox_register_device(struct vmbox_device *dev);
int vmbox_register_driver(struct vmbox_driver *drv);
void vmbox_unregister_driver(struct vmbox_driver *drv);

int vmbox_device_init(struct vmbox_device *vdev, unsigned long flags);

void *vmbox_device_remap(struct vmbox_device *vdev);
void vmbox_device_unmap(struct vmbox_device *vdev);

int vmbox_device_ipc_event(struct vmbox_device *vdev, int event);
int vmbox_device_vring_event(struct vmbox_device *vdev);

void vmbox_device_state_change(struct vmbox_device *vdev, int state);

static int inline vmbox_device_is_backend(struct vmbox_device *vdev)
{
	return (vdev->flags & VMBOX_F_DEV_BACKEND);
}

int vmbox_device_online(struct vmbox_device *vdev);
int vmbox_device_ipc_event(struct vmbox_device *vdev, int event);

static void inline vmbox_device_offline(struct vmbox_device *vdev)
{
	vmbox_device_state_change(vdev, VMBOX_DEV_STAT_OFFLINE);
}

#define module_vmbox_driver(__vmbox_driver) \
	module_driver(__vmbox_driver, vmbox_register_driver, \
			vmbox_unregister_driver)

#endif
