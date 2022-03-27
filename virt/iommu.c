// SPDX-License-Identifier: GPL-2.0

#include <minos/errno.h>
#include <minos/of.h>
#include <virt/vm.h>
#include <virt/iommu.h>

static const struct iommu_ops *iommu_ops;

int iommu_vm_init(struct vm *vm)
{
	struct vm_iommu *iommu = &vm->iommu;
	int ret;

	init_list(&iommu->nodes);

	iommu->ops = iommu_ops;

	if (!iommu->ops || !iommu->ops->vm_init)
		return 0;

	ret = iommu->ops->vm_init(vm);

	return ret;
}

int iommu_iotlb_flush_all(struct vm *vm)
{
	struct vm_iommu *iommu = &vm->iommu;
	int ret;

	if (!iommu->ops || !iommu->ops->iotlb_flush_all)
		return 0;

	/* TODO: need to hold mm_lock? */
	ret = iommu->ops->iotlb_flush_all(vm);
	if (ret)
		pr_err("vm%d: IOMMU IOTLB flush all failed: %d\n",
				vm_id(vm), ret);

	return ret;
}

int iommu_assign_node(struct vm *vm, struct device_node *node)
{
	struct vm_iommu *iommu = &vm->iommu;
	int ret;
	char name[32];
	struct device_node *stub_node;

	ret = of_get_string(node, "minos,stub-node", name, ARRAY_SIZE(name));
	if (ret <= 0)
		return 0;

	stub_node = of_find_node_by_name(of_root_node, name);
	if (!stub_node)
		return -ENOENT;

	if (!iommu->ops || !iommu->ops->assign_node)
		return 0;

	pr_info("[VM%d NODE] %s <- %s\n", vm_id(vm), devnode_name(stub_node),
		devnode_name(node));

	ret = iommu->ops->assign_node(vm, stub_node);

	return ret;
}

static void *iommu_ops_init(struct device_node *node, void *arg)
{
	extern unsigned char __iommu_ops_start;
	extern unsigned char __iommu_ops_end;
	void *s, *e;
	struct iommu_ops *ops;

	s = (void *)&__iommu_ops_start;
	e = (void *)&__iommu_ops_end;

	ops = (struct iommu_ops *)of_device_node_match(node, s, e);
	if (!ops)
		return NULL;

	/*
	 * Some platforms have tree-like IOMMUs, so this function may be
	 * called multiple times.
	 */
	if (!iommu_ops)
		iommu_ops = ops;
	else if (iommu_ops != ops)
		pr_warn("Cannot set IOMMU ops, already set to a different value\n");

	if (ops->init)
		ops->init(node);

	return node;
}

static void of_iommu_init(void)
{
	of_iterate_all_node_loop(of_root_node, iommu_ops_init, NULL);
}

static int iommu_init(void)
{
#ifdef CONFIG_DEVICE_TREE
	of_iommu_init();
#endif

	return 0;
}

subsys_initcall(iommu_init);
