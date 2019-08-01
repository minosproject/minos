#ifndef __MINOS_VIRT_VMM_H__
#define __MINOS_VIRT_VMM_H__

#include <minos/types.h>
#include <minos/mm.h>
#include <minos/mmu.h>
#include <virt/vm_mmap.h>

struct vm;

int register_memory_region(struct memtag *res);
int vm_mm_init(struct vm *vm);

int map_vm_memory(struct vm *vm, unsigned long vir_base,
		unsigned long phy_base, size_t size, int type);
int unmap_vm_memory(struct vm *vm, unsigned long vir_addr,
			size_t size, int type);

int alloc_vm_memory(struct vm *vm, unsigned long start, size_t size);
void release_vm_memory(struct vm *vm);

int create_guest_mapping(struct vm *vm, unsigned long vir,
		unsigned long phy, size_t size, unsigned long flags);

int vm_mmap(struct vm *vm, unsigned long offset, unsigned long size);
void vm_unmmap(struct vm *vm);
int vm_mmap_init(struct vm *vm, size_t memsize);
void *vm_alloc_pages(struct vm *vm, int pages);

unsigned long create_hvm_iomem_map(unsigned long phy, uint32_t size);
void destroy_hvm_iomem_map(unsigned long vir, uint32_t size);
int create_early_pmd_mapping(unsigned long vir, unsigned long phy);

void *map_vm_mem(unsigned long gva, size_t size);
void unmap_vm_mem(unsigned long gva, size_t size);

void *vm_map_shmem(struct vm *vm, void *phy, uint32_t size,
		unsigned long flags);
void vm_init_shmem(struct vm *vm, uint64_t base, uint64_t size);


phy_addr_t get_vm_memblock_address(struct vm *vm, unsigned long a);

#endif
