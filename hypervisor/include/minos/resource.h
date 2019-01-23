#ifndef __MINOS_RESOURCE_H__
#define __MINOS_RESOURCE_H__

#include <minos/types.h>

struct vm;

int translate_device_address_index(struct device_node *node,
		uint64_t *base, uint64_t *size, int index);
int translate_device_address(struct device_node *node,
		uint64_t *base, uint64_t *size);
int get_device_irq_index(struct vm *vm, struct device_node *node,
		uint32_t *irq, unsigned long *flags, int index);

int create_vm_resource_of(struct vm *vm, void *data);

#endif
