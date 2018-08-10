#ifndef __MVM_MVM_DEVICE_H__
#define __MVM_MVM_DEVICE_H__

#include <mvm.h>

#define PDEV_NAME_SIZE		(31)

struct mvm_device;

struct dev_ops {
	char *name;
	int (*dev_init)(struct mvm_device *, char *);
	int (*dev_deinit)(struct mvm_device *);
	int (*handle_event)(struct mvm_device *);
};

struct mvm_device {
	struct vm *vm;
	void *iomem;
	void *pdata;
	void *drv_data;
	int gvm_irq;
	int hvm_irq;
	void *guest_iomem;
	int guest_visable;
	struct dev_ops *ops;
	char name[PDEV_NAME_SIZE + 1];
	struct list_head list;
};

#define DEFINE_MDEV_TYPE(ops)	\
	static void *mdev_ops_##ops __used __section("mdev_ops") = &ops

extern unsigned char __start_mdev_ops;
extern unsigned char __stop_mdev_ops;

#endif
