#ifndef __MINOS_VIRT_VMM_H__
#define __MINOS_VIRT_VMM_H__

#include <minos/types.h>
#include <minos/mm.h>
#include <minos/mmu.h>
#include <minos/vm_mmap.h>

struct vm;

/*
 * pgd_base : the lvl0 table base
 * mem_list : static config memory region for this vm
 * block_list : the mem_block allocated for this vm
 * head : the pages table allocated for this vm
 */
struct mm_struct {
	size_t mem_size;
	size_t mem_free;
	unsigned long mem_base;
	unsigned long pgd_base;
	unsigned long hvm_mmap_base;
	struct page *head;
	struct list_head mem_list;
	struct list_head block_list;
	spinlock_t lock;
};

int register_memory_region(struct memtag *res);
int vm_mm_init(struct vm *vm);

int map_vm_memory(struct vm *vm, unsigned long vir_base,
		unsigned long phy_base, size_t size, int type);
int unmap_vm_memory(struct vm *vm, unsigned long vir_addr,
			size_t size, int type);

int alloc_vm_memory(struct vm *vm, unsigned long start, size_t size);
void release_vm_memory(struct vm *vm);

int create_host_mapping(unsigned long vir, unsigned long phy,
		size_t size, unsigned long flags);
int destroy_host_mapping(unsigned long vir, size_t size);

static inline int
io_remap(unsigned long vir, unsigned long phy, size_t size)
{
	return create_host_mapping(vir, phy, size, VM_IO);
}

static inline int
io_unmap(unsigned long vir, size_t size)
{
	return destroy_host_mapping(vir, size);
}

int vm_mmap(struct vm *vm, unsigned long offset, unsigned long size);
void vm_unmmap(struct vm *vm);
int vm_mmap_init(struct vm *vm, size_t memsize);

#endif
