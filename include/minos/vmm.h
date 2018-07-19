#ifndef __MINOS_VIRT_VMM_H__
#define __MINOS_VIRT_VMM_H__

#include <minos/virt.h>
#include <minos/vmm.h>

int register_memory_region(struct memtag *res);
int vm_mm_init(struct vm *vm);
int map_vm_memory(struct vm *vm, unsigned long vir_base,
		unsigned long phy_base, size_t size, int type);
void *get_vm_translation_page(struct vm *vm);

#endif
