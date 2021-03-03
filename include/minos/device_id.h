#ifndef _MINOS_DEVICE_ID_H_
#define _MINOS_DEVICE_ID_H_

#include <minos/types.h>
#include <minos/init.h>

typedef enum __device_class {
	DT_CLASS_CPU = 0,
	DT_CLASS_MEMORY,
	DT_CLASS_IRQCHIP,
	DT_CLASS_TIMER,
	DT_CLASS_SIMPLE_BUS,
	DT_CLASS_PCI_BUS,
	DT_CLASS_VDEV,
	DT_CLASS_PDEV,
	DT_CLASS_VM,
	DT_CLASS_VMBOX,
	DT_CLASS_VIRQCHIP,
	DT_CLASS_OTHER,
} device_class_t;

#define DEVICE_NODE_F_ROOT		(1 << 0)
#define DEVICE_NODE_F_OF		(1 << 1)

struct node_iommu {
	/* private information for iommu drivers */
	void *priv;
};

/*
 * data       - the data for all device such as dtb or acpi
 * offset     - node offset
 * name       - the name of the device node
 * compatible - the compatible used to match device
 * parent     - the parent node of device_node
 * child      - child nodes of the device_node
 * sibling    - brother of the device node
 * iommu      - information for iommu
 */
struct device_node {
	void *data;
	int offset;
	const char *name;
	const char *compatible;
	struct device_node *parent;
	struct device_node *child;
	struct device_node *sibling;
	struct device_node *next;
	device_class_t class;
	unsigned long flags;
	struct node_iommu iommu;
};

#define devnode_name(node)	node->name

struct module_id {
	const char *name;
	char **comp;
	void *data;
};

#define MINOS_MODULE_DECLARE(mname, mn, init_fn) \
	static const struct module_id __used \
	module_match_##mname __section(.__vmodule) = { \
		.name = mn, \
		.comp = NULL, \
		.data = init_fn, \
	}

#define IRQCHIP_DECLARE(mname, mn, irqchip) \
	static const struct module_id __used \
	module_match_##mname __section(.__irqchip) = { \
		.comp = mn, \
		.data = irqchip, \
	}

#define IOMMU_OPS_DECLARE(mname, mn, iommu_ops) \
	static const struct module_id __used \
	module_match_##mname __section(.__iommu_ops) = { \
		.comp = mn, \
		.data = iommu_ops, \
	}

#define VIRQCHIP_DECLARE(mname, mn, virqchip) \
	static const struct module_id __used \
	module_match_##mname __section(.__virqchip) = { \
		.comp = mn, \
		.data = virqchip, \
	}

#define VDEV_DECLARE(mname, mn, vdev) \
	static const struct module_id __used \
	module_match_##mname __section(.__vdev) = { \
		.comp = mn, \
		.data = vdev, \
	}

extern char *gicv2_match_table[];
extern char *gicv3_match_table[];
extern char *bcmirq_match_table[];
extern char *ipmmu_match_table[];
extern char *pl031_match_table[];
extern char *sp805_match_table[];
extern char *virtio_match_table[];
extern char *aic_match_table[];
extern char *arm_arch_timer_match_table[];

#endif
