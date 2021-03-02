// SPDX-License-Identifier: GPL-2.0

#ifndef _MINOS_IOMMU_H_
#define _MINOS_IOMMU_H_

struct iommu_ops {
	int (*init)(struct device_node *node);
	int (*vm_init)(struct vm *vm);
	int (*vm_destroy)(struct vm *vm);
	int (*iotlb_flush_all)(struct vm *vm);
	int (*assign_node)(struct vm *vm, struct device_node *node);
	int (*deassign_node)(struct vm *vm, struct device_node *node);
};

int iommu_vm_init(struct vm *vm);

int iommu_vm_destroy(struct vm *vm);

int iommu_iotlb_flush_all(struct vm *vm);

int iommu_assign_node(struct vm *vm, struct device_node *node);

int iommu_deassign_node(struct vm *vm, struct device_node *node);

#endif
