#ifndef __MINOS_VIRT_VMM_H__
#define __MINOS_VIRT_VMM_H__

#include <minos/types.h>
#include <minos/mm.h>
#include <virt/vm_mmap.h>
#include <virt/iommu.h>

struct vm;

struct mem_block {
	uint32_t bfn;
	struct mem_block *next;
};

struct vm_iommu {
	/* private information for iommu drivers */
	void *priv;

	const struct iommu_ops *ops;

	/* list of device nodes assigned to this vm */
	struct list_head nodes;
};

/*
 * pstart - if this area is mapped as continous the pstart
 * is the phsical address of this vmm_area
 */
struct vmm_area {
	unsigned long start;
	unsigned long end;
	unsigned long pstart;
	size_t size;
	int flags;
	int vmid;			/* 0 - for self other for VM */
	struct list_head list;
	struct mem_block *b_head;

	/* if this vmm_area is belong to VDEV, this will link
	 * to the next vmm_area of the VDEV */
	struct vmm_area *next;
};

struct mm_struct {
	void *pgdp;
	spinlock_t lock;

	/*
	 * vmm_area_free : list to all the free vmm_area
	 * vmm_area_used : list to all the used vmm_area
	 * lock		 : spin lock for vmm_area allocate
	 */
	struct vmm_area *mem_va;
	struct list_head vmm_area_free;
	struct list_head vmm_area_used;
};

int vm_mm_init(struct vm *vm);
int vm_mm_struct_init(struct vm *vm);

int copy_from_guest(void *target, void __guest *source, size_t size);

int alloc_vm_memory(struct vm *vm);
void release_vm_memory(struct vm *vm);

int create_guest_mapping(struct mm_struct *mm, unsigned long vir,
		unsigned long phy, size_t size, unsigned long flags);

struct vmm_area *vm_mmap(struct vm *vm, unsigned long offset,
		unsigned long size);

void *vm_alloc_pages(struct vm *vm, int pages);

unsigned long create_hvm_shmem_map(struct vm *vm,
		unsigned long phy, uint32_t size);

void destroy_hvm_iomem_map(unsigned long vir, uint32_t size);
int create_early_pmd_mapping(unsigned long vir, unsigned long phy);

struct vmm_area *split_vmm_area(struct mm_struct *mm, unsigned long base,
		unsigned long size, unsigned long flags);

struct vmm_area *request_vmm_area(struct mm_struct *mm, unsigned long base,
		unsigned long pbase, size_t size,
		unsigned long flags);

int map_vmm_area(struct mm_struct *mm, struct vmm_area *va,
		unsigned long pbase);

struct vmm_area *alloc_free_vmm_area(struct mm_struct *mm,
		size_t size, unsigned long mask, unsigned long flags);

int unmap_vmm_area(struct mm_struct *mm, struct vmm_area *va);

struct mem_block *vmm_alloc_memblock(void);
int vmm_free_memblock(struct mem_block *mb);
int vmm_has_enough_memory(size_t size);

int release_vmm_area(struct mm_struct *mm, struct vmm_area *va);

int translate_guest_ipa(struct mm_struct *mm,
		unsigned long offset, unsigned long *pa);

void free_shmem(void *addr);
void *alloc_shmem(int pages);

#endif
