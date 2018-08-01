#ifndef __MINOS_VIRT_VMM_H__
#define __MINOS_VIRT_VMM_H__

#include <minos/types.h>
#include <minos/mm.h>
#include <minos/mmu.h>

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

#define VM0_MMAP_REGION_START		(0xc0000000)
#define VM0_MMAP_REGION_SIZE		(0x40000000)

#define VM_MMAP_MAX_SIZE		(VM0_MMAP_REGION_SIZE / CONFIG_MAX_VM)
#define VM_MMAP_ENTRY_COUNT		(VM_MMAP_MAX_SIZE >> MEM_BLOCK_SHIFT)

struct vm;

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

int map_vm_memory(struct vm *vm, unsigned long vir_base,
		unsigned long phy_base, size_t size, int type);
int unmap_vm_memory(struct vm *vm, unsigned long vir_addr,
			size_t size, int type);

int alloc_vm_memory(struct vm *vm, unsigned long start, size_t size);
void release_vm_memory(struct vm *vm);

int create_host_mapping(unsigned long, unsigned long, size_t, int);
int destroy_host_mapping(unsigned long, size_t, int);

static inline int
io_remap(unsigned long vir, unsigned long phy, size_t size)
{
	return create_host_mapping(vir, phy, size, MEM_TYPE_IO);
}

static inline int
io_unmap(unsigned long vir, size_t size)
{
	return destroy_host_mapping(vir, size, MEM_TYPE_IO);
}

static inline void create_guest_level_mapping(int lvl, unsigned long tt,
			unsigned long value, int map_type)
{
	create_level_mapping(lvl, tt, value, MEM_TYPE_NORMAL, map_type, 0);
}

static inline void create_host_level_mapping(int lvl, unsigned long tt,
			unsigned long value, int map_type)
{
	create_level_mapping(lvl, tt, value, MEM_TYPE_NORMAL, map_type, 1);
}

unsigned long get_vm_mmap_info(int vmid, unsigned long *size);
int vm_mmap(struct vm *vm, unsigned long offset, unsigned long size);
void vm_unmmap(struct vm *vm);

#endif
