#ifndef __MINOS_VIRT_VMM_H__
#define __MINOS_VIRT_VMM_H__

#include <minos/virt.h>

int register_memory_region(struct memtag *res);
int vm_mm_init(struct vm *vm);

#endif
