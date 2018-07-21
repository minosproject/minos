#ifndef __MINOS_VIRT_VMM_H__
#define __MINOS_VIRT_VMM_H__

#include <minos/types.h>
#include <minos/virt.h>
#include <minos/mm.h>

/*
 * below is the memory map for guest vm
 * - max physic size 1T
 * - 0x0 - 0x3fcfffff is the resve memory space
 * - 0x3fd00000 - 0x40000000 is the shared memory space
 * - 0x40000000 - 0x7fffffff is the io space
 * - 0x80000000 - 0x80000000 + 512G is the normal memory
 */

#define GUSET_PHYSIC_MEM_MAX_SIZE	(1UL << 40)
#define GUEST_PHYSIC_MEM_START		(0x0000000000000000UL)

#define GUEST_SHARED_MEM_START		(0x40000000 - 0x200000)
#define GUEST_SHARED_MEM_SIZE		(0x200000)

#define GUEST_IO_MEM_START		(0x40000000UL)
#define GUEST_IO_MEM_SIZE		(0x40000000UL)

#define GUEST_NORMAL_MEM_START		(GUEST_IO_MEM_START + GUEST_IO_MEM_SIZE)
#define GUSET_NORMAL_MEM_SIZE		(1UL << 39)
#define GUSET_MEMORY_END		(GUEST_NORMAL_MEM_START + GUSET_PHYSIC_MEM_MAX_SIZE)

#define VM0_DEFAULT_SHMEM_SIZE		(PAGE_SIZE)

/*
 * page_table_base : the lvl0 table base
 * mem_list : static config memory region for this vm
 * block_list : the mem_block allocated for this vm
 * head : the pages table allocated for this vm
 */
struct mm_struct {
	size_t mem_size;
	size_t mem_free;
	unsigned long mem_base;
	unsigned long page_table_base;
	struct page *head;
	struct list_head mem_list;
	struct list_head block_list;
	spinlock_t lock;
};

int register_memory_region(struct memtag *res);
int vm_mm_init(struct vm *vm);
void *get_vm_translation_page(struct vm *vm);
int map_vm_memory(struct vm *vm, unsigned long vir_base,
		unsigned long phy_base, size_t size, int type);
int unmap_vm_memory(struct vm *vm, unsigned long vir_addr,
			size_t size, int type);
int alloc_vm_memory(struct vm *vm, unsigned long start, size_t size);

void release_vm_memory(struct vm *vm);

#endif
