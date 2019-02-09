/*
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <minos/minos.h>
#include <minos/list.h>
#include <minos/of.h>
#include <minos/vm.h>
#include <minos/vdev.h>
#include <minos/virq_chip.h>
#include <minos/mm.h>
#include <minos/vmm.h>
#include <minos/irq.h>

static void *virqchip_start;
static void *virqchip_end;
static void *vdev_start;
static void *vdev_end;

int translate_device_address_index(struct device_node *node,
		uint64_t *base, uint64_t *size, int index)
{
	if (node->flags & DEVICE_NODE_F_OF)
		return of_translate_address_index(node, base, size, index);

	return -EINVAL;
}

int translate_device_address(struct device_node *node,
		uint64_t *base, uint64_t *size)
{
	return translate_device_address_index(node, base, size, 0);
}

static int create_vm_vdev_of(struct vm *vm, struct device_node *node)
{
	vdev_init_t func;

	if (!node->compatible)
		return -EINVAL;

	func = (vdev_init_t)of_device_node_match(node,
			vdev_start, vdev_end);
	if (!func) {
		pr_warn("can not find vdev-%s\n", node->compatible);
		return -ENOENT;
	}

	/* create the vdev */
	func(vm, node);

	return 0;
}

static void *create_vm_irqchip_of(struct device_node *node, void *arg)
{
	struct vm *vm = (struct vm *)arg;
	vdev_init_t func;

	if (node->class != DT_CLASS_IRQCHIP)
		return NULL;
	if (vm->virq_chip != NULL) {
		pr_warn("virq chip has been created\n");
		return NULL;
	}

	func = (vdev_init_t)of_device_node_match(node,
			virqchip_start, virqchip_end);
	if (!func) {
		pr_warn("virq-chip :%s not found\n", node->compatible);
		node->class = DT_CLASS_PDEV;
		return NULL;
	}

	vm->virq_chip = func(vm, (void *)node);
	if (!vm->virq_chip)
		return NULL;

	pr_info("create virq_chip %s successfully\n", node->compatible);

	return node;
}

static int create_pdev_virq_of(struct vm *vm, struct device_node *node)
{
	of32_t *val;
	of32_t *virq_affinity = NULL;
	uint32_t irqs[4], irq, hw_irq = 0;
	int i, j, nr_icells, hw = 0;
	int len, len_aff, ret;
	struct virq_chip *vc = vm->virq_chip;
	unsigned long type;

	val = (of32_t *)of_getprop(node, "interrupts", &len);
	if (!val || (len < 4))
		return -EINVAL;

	/* get the irq count for the device */
	nr_icells = of_n_interrupt_cells(node);
	if (nr_icells == 0)
		return 0;

	len = len / 4;
	if (vm_is_native(vm))
		hw = 1;
	else {
		virq_affinity = (of32_t *)of_getprop(node, "virq_affinity", &len_aff);
		if (virq_affinity && (len_aff == len))
			hw = 1;
	}

	for (i = 0; i < len; i += nr_icells) {
		for (j = 0; j < nr_icells; j++)
			irqs[j] = of32_to_cpu(val[i + j]);

		ret = vc->xlate(node, irqs, nr_icells, &irq, &type);
		if (ret)
			continue;

		if (hw) {
			if (vm_is_native(vm))
				hw_irq = irq;
			else
				hw_irq = of32_to_cpu(virq_affinity[i / nr_icells]);
		}

		/* register the hardware irq for vm */
		pr_info("[VM%d VIRQ] %d->%d %s\n", vm->vmid,
				irq, hw_irq, node->name);
		request_hw_virq(vm, irq, hw_irq, 0);
	}

	return 0;
}

static int create_pdev_iomem_of(struct vm *vm, struct device_node *node)
{
	uint64_t addr, size;
	int i, nr_addr, ret;

	/* get the count of memory region for the device */
	nr_addr = of_n_addr_count(node);

	for (i = 0; i < nr_addr; i++) {
		ret = of_translate_address_index(node, &addr, &size, i);
		if (ret) {
			pr_warn("bad address or size for %s\n", i, node->name);
			continue;
		}

		if (size == 0)
			continue;

		/* map the physical memory for vm */
		pr_info("[VM%d IOMEM] 0x%x->0x%x 0x%x %s\n", vm->vmid,
				addr, addr, size, node->name);
		create_guest_mapping(vm, addr, addr, size, VM_IO);
	}

	return 0;
}

int get_device_irq_index(struct vm *vm, struct device_node *node,
		uint32_t *irq, unsigned long *flags, int index)
{
	int irq_cells, len, i;
	of32_t *value;
	uint32_t irqv[4];

	if (!node)
		return -EINVAL;

	value = (of32_t *)of_getprop(node, "interrupts", &len);
	if (!value || (len < sizeof(of32_t)))
		return -ENOENT;

	irq_cells = of_n_interrupt_cells(node);
	if (irq_cells == 0) {
		pr_error("bad irqcells - %s\n", node->name);
		return -ENOENT;
	}

	pr_debug("interrupt-cells %d\n", irq_cells);

	len = len / sizeof(of32_t);
	if (index >= len)
		return -ENOENT;

	value += (index * irq_cells);
	for (i = 0; i < irq_cells; i++)
		irqv[i] = of32_to_cpu(*value++);

	if (vm && vm->virq_chip)
		return vm->virq_chip->xlate(node, irqv, irq_cells, irq, flags);
	else
		return irq_xlate(node, irqv, irq_cells, irq, flags);

	return -EINVAL;
}

static int create_vm_pdev_of(struct vm *vm, struct device_node *node)
{
	int ret = 0;

	ret += create_pdev_iomem_of(vm, node);
	ret += create_pdev_virq_of(vm, node);

	return ret;
}

static void *__create_vm_resource_of(struct device_node *node, void *arg)
{
	struct vm *vm = (struct vm *)arg;

	switch(node->class) {
		case DT_CLASS_PCI_BUS:
		case DT_CLASS_PDEV:
			create_vm_pdev_of(vm, node);
			break;
		case DT_CLASS_VDEV:
			create_vm_vdev_of(vm, node);
			break;
		default:
			break;
	}

	return NULL;
}

int create_vm_resource_of(struct vm *vm, void *data)
{
	struct device_node *node;

	if (!vm)
		return -EINVAL;

	if (vm_is_hvm(vm))
		node = hv_node;
	else
		node = of_parse_device_tree(data);
	if (!node) {
		pr_error("invaild setup data for vm-%d\n", vm->vmid);
		return -EINVAL;
	}

	/*
	 * first of all need to create the virq controller
	 * of this vm
	 */
	of_iterate_all_node(node, create_vm_irqchip_of, vm);
	if (!vm->virq_chip)
		panic("can not create virq chip for vm\n");

	of_iterate_all_node_loop(node, __create_vm_resource_of, vm);

	/* here we can free all the device node to save memory */
	of_release_all_node(node);
	if (vm_is_hvm(vm))
		hv_node = NULL;

	return 0;
}

static int resource_init(void)
{
	extern unsigned char __virqchip_start;
	extern unsigned char __virqchip_end;
	extern unsigned char __vdev_start;
	extern unsigned char __vdev_end;

	virqchip_start = (void *)&__virqchip_start;
	virqchip_end = (void *)&__virqchip_end;
	vdev_start = (void *)&__vdev_start;
	vdev_end = (void *)&__vdev_end;

	return 0;
}
arch_initcall(resource_init);
