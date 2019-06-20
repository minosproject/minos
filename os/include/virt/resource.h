#ifndef __MINOS_RESOURCE_H__
#define __MINOS_RESOURCE_H__

#include <minos/types.h>

struct vm;

int create_vm_resource_of(struct vm *vm, void *data);

#endif
