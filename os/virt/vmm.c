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
#include <virt/vm.h>
#include <minos/mmu.h>

extern unsigned char __el2_ttb0_pgd;
extern unsigned char __el2_ttb0_pud;
extern unsigned char __el2_ttb0_pmd_code;
extern unsigned char __el2_ttb0_pmd_io;

static unsigned long alloc_pgd(void)
{
	/*
	 * return the table base address, this function
	 * is called when init the vm
	 *
	 * 2 pages for each VM to map 1T IPA memory
	 *
	 */
	void *page;

	page = __get_free_pages(GVM_PGD_PAGE_NR,
			GVM_PGD_PAGE_ALIGN);
	if (!page)
		panic("No memory to map vm memory\n");

	memset(page, 0, SIZE_4K * GVM_PGD_PAGE_NR);
	return (unsigned long)page;
}

void *vm_alloc_pages(struct vm *vm, int pages)
{
	struct page *page;
	struct mm_struct *mm = &vm->mm;

	page = alloc_pages(pages);
	if (!pages)
		return NULL;

	spin_lock(&vm->mm.mm_lock);
	page->next = mm->page_head;
	mm->page_head = page;
	spin_unlock(&vm->mm.mm_lock);

	return page_to_addr(page);
}

int create_guest_mapping(struct mm_struct *mm, vir_addr_t vir,
		phy_addr_t phy, size_t size, unsigned long flags)
{
	unsigned long tmp;
	struct vm *vm = mm->vm;

	tmp = BALIGN(vir + size, PAGE_SIZE);
	vir = ALIGN(vir, PAGE_SIZE);
	phy = ALIGN(phy, PAGE_SIZE);
	size = tmp - vir;

	pr_debug("map 0x%x->0x%x size-0x%x vm-%d\n", vir,
			phy, size, vm->vmid);

	return create_mem_mapping(mm, vir, phy, size, flags);
}

static int __used destroy_guest_mapping(struct mm_struct *mm,
		unsigned long vir, size_t size)
{
	unsigned long end;

	end = vir + size;
	end = BALIGN(end, PAGE_SIZE);
	vir = ALIGN(vir, PAGE_SIZE);
	size = end - vir;

	return destroy_mem_mapping(mm, vir, size, 0);
}

static struct vmm_area *__alloc_vmm_area_entry(unsigned long base,
		unsigned long pbase, size_t size, unsigned long flags)
{
	struct vmm_area *va;

	va = zalloc(sizeof(*va));
	if (!va)
		return NULL;

	va->start = base;
	va->pstart = pbase;
	va->end = base + size - 1;
	va->size = size;
	va->flags = flags;

	return va;
}

static int add_used_vmm_area(struct mm_struct *mm, struct vmm_area *va)
{
	if (!va)
		return -EINVAL;

	list_add_tail(&mm->vmm_area_used, &va->list);
	return 0;
}

static int add_free_vmm_area(struct mm_struct *mm, struct vmm_area *area)
{
	size_t size;
	struct vmm_area *tmp, *va = area;

	if (!va)
		return -EINVAL;

	/* indicate it not inserted to the free list */
	va->list.next = NULL;
	size = va->size;
	va->flags = 0;
	va->vmid = 0;

repeat:
	/*
	 * check whether this two vmm_area can merged to one
	 * vmm_area and do the action
	 */
	list_for_each_entry(tmp, &mm->vmm_area_free, list) {
		if (va->start == (tmp->end + 1)) {
			tmp->size += va->size;
			list_del(&tmp->list);
			free(va);
			va = tmp;
			goto repeat;
		}

		if ((va->end + 1) == tmp->start) {
			va->size += tmp->size;
			list_del(&tmp->list);
			free(tmp);
			goto repeat;
		}

		if (size <= tmp->size) {
			list_insert_before(&tmp->list, &va->list);
			break;
		}
	}

	if (va->list.next == NULL)
		list_add_tail(&mm->vmm_area_free, &va->list);

	return 0;
}

int release_vmm_area(struct mm_struct *mm, struct vmm_area *va)
{
	int ret;

	spin_lock(&mm->vmm_area_lock);
	list_del(&va->list);
	ret = add_free_vmm_area(mm, va);
	spin_unlock(&mm->vmm_area_lock);

	return ret;
}

static int create_free_vmm_area(struct mm_struct *mm, unsigned long base,
		unsigned long size, unsigned long flags)
{
	int ret;
	struct vmm_area *va;

	if (!IS_PAGE_ALIGN(base) || !IS_PAGE_ALIGN(size)) {
		pr_err("vm_area is not page align 0x%p 0x%x\n",
				base, size);
		return -EINVAL;
	}

	va = __alloc_vmm_area_entry(base, 0, size, flags);
	if (!va) {
		pr_err("failed to alloc free vmm_area\n");
		return -ENOMEM;
	}

	spin_lock(&mm->vmm_area_lock);
	ret = add_free_vmm_area(mm, va);
	spin_unlock(&mm->vmm_area_lock);

	return ret;
}

static inline int vmm_area_map_ln(struct mm_struct *mm, struct vmm_area *va)
{
	return create_guest_mapping(mm, va->start,
			va->pstart, va->size, va->flags);
}

static inline int vmm_area_map_bk(struct mm_struct *mm, struct vmm_area *va)
{
	int ret;
	struct mem_block *block;
	unsigned long base = va->start;
	unsigned long size = va->size;

	list_for_each_entry(block, &va->b_head, list) {
		ret = create_guest_mapping(mm, base, block->phy_base,
				MEM_BLOCK_SIZE, va->flags);
		if (ret)
			return ret;

		base += MEM_BLOCK_SIZE;
		size -= MEM_BLOCK_SIZE;

		if (size == 0)
			break;
	}

	return 0;
}

static inline int vmm_area_map_pg(struct mm_struct *mm, struct vmm_area *va)
{
	int ret;
	struct page *page = va->p_head;
	unsigned long base = va->start;
	unsigned long size = va->size;

	do {
		ret = create_guest_mapping(mm, base, page->phy_base & ~PAGE_MASK,
				PAGE_SIZE, va->flags);
		if (ret)
			return ret;

		size -= PAGE_SIZE;
		page = page->next;

		if (page == NULL)
			break;
	} while (size > 0);

	return 0;
}

int map_vmm_area(struct mm_struct *mm,
		struct vmm_area *va, unsigned long pbase)
{
	if (pbase && IS_PAGE_ALIGN(pbase)) {
		va->pstart = pbase;
		va->flags |= VM_LN;
	}

	if (!(va->pstart) && !(va->flags & VM_MAP_TYPE_MASK)) {
		pr_warn("wrong vmm_area flags\n");
		return -EINVAL;
	}

	switch (va->flags & VM_MAP_TYPE_MASK) {
	case VM_LN:
	case VM_PT:
		vmm_area_map_ln(mm, va);
		break;
	case VM_BK:
		vmm_area_map_bk(mm, va);
		break;
	case VM_PG:
		vmm_area_map_pg(mm, va);
		break;
	default:
		pr_warn("unkown vmm_area map type 0x%p\n",
				va->flags & VM_MAP_TYPE_MASK);
		break;
	}

	return 0;
}

static struct vmm_area *__alloc_free_vmm_area(struct mm_struct *mm,
		struct vmm_area *fva, size_t size,
		unsigned long mask, unsigned long flags)
{
	unsigned long va_base = fva->start;
	unsigned long new_base, new_size;
	struct vmm_area *old = NULL, *old1 = NULL, *new = NULL;

	/*
	 * allocate a free vma from another vma area, first need to
	 * check the start address is the address type we needed, need
	 * to check
	 */
	if (!(va_base & mask)) {
		new = __alloc_vmm_area_entry(fva->start, 0, size, flags);
		if (!new)
			return NULL;

		if (size < fva->size) {
			fva->start = new->end + 1;
			fva->size = fva->size - size;
			old = fva;
		}

		goto out;
	}

	new_base = (va_base + mask) & ~mask;
	new_size = fva->size - (new_base - va_base);

	if (new_size < size)
		return NULL;

	/* split the header of the va to a new va */
	old1 = __alloc_vmm_area_entry(fva->start,
			0, new_base - va_base, 0);
	fva->size -= (new_base - va_base);
	fva->start += (new_base - va_base);

	if (fva->size > size) {
		new = __alloc_vmm_area_entry(fva->start, 0, size, flags);
		if (!new)
			return NULL;

		old = fva;
		old->start = old->start + size;
		old->size = old->size - size;
		old->end = old->start + old->size - 1;
	} else if (fva->size == size) {
		new = fva;
		new->flags = flags;
	}

out:
	list_del(&fva->list);
	if (old1)
		add_free_vmm_area(mm, old1);
	if (old)
		add_free_vmm_area(mm, old);
	if (new)
		add_used_vmm_area(mm, new);

	return new;
}

struct vmm_area *alloc_free_vmm_area(struct mm_struct *mm,
		size_t size, unsigned long mask, unsigned long flags)
{
	struct vmm_area *va;
	struct vmm_area *new = NULL;

	size = BALIGN(size, PAGE_SIZE);
	if ((mask != PAGE_MASK) && (mask != BLOCK_MASK))
		mask = PAGE_MASK;

	if (mask == BLOCK_MASK)
		size = BALIGN(size, MEM_BLOCK_SIZE);

	spin_lock(&mm->vmm_area_lock);
	list_for_each_entry(va, &mm->vmm_area_free, list) {
		if (va->size < size)
			continue;

		new = __alloc_free_vmm_area(mm, va, size, mask, flags);
		if (new)
			break;
	}
	spin_unlock(&mm->vmm_area_lock);

	return new;
}

int split_vmm_area(struct mm_struct *mm, unsigned long base,
		unsigned long pbase, unsigned long size, unsigned long flags)
{
	unsigned long start, end;
	unsigned long new_end = base + size;
	struct vmm_area *va;
	struct vmm_area *new = NULL;
	struct vmm_area *old = NULL;
	struct vmm_area *old1 = NULL;

	if (!IS_PAGE_ALIGN(base) || !IS_PAGE_ALIGN(size) ||
			(size == 0) || !IS_PAGE_ALIGN(pbase)) {
		pr_err("vm_area is not page align 0x%p 0x%x\n",
				base, size);
		return -EINVAL;
	}

	spin_lock(&mm->vmm_area_lock);
	list_for_each_entry(va, &mm->vmm_area_free, list) {
		start = va->start;
		end = va->end + 1;

		if ((base > end) || (base < start) || (new_end > end))
			continue;

		if ((base == start) && (new_end == end)) {
			new = va;
			new->flags = flags;
		} else if ((base == start) && new_end < end) {
			old = va;
			va->start = new_end;
			va->size -= size;
		} else if ((base > start) && (new_end < end)) {
			/* allocate a new vmm_area for right free */
			old1 = __alloc_vmm_area_entry(base, pbase, size, flags);
			if (!old1)
				panic("no more memory for vmm_area\n");

			old1->start = new_end;
			old1->size = end - new_end;
			old1->end = old1->start + old1->size - 1;
			old1->flags = va->flags;

			old = va;
			va->size = base - start;
			va->end = va->start + va->size - 1;
		} else if ((base > start) && end == new_end) {
			old = va;
			va->size = va->size - size;
		}

		new = __alloc_vmm_area_entry(base, pbase, size, flags);
		if (!new)
			panic("no more memory for vmm_area\n");

		break;
	}

	if ((old == NULL) && (new == NULL)) {
		pr_err("invalid vmm_area config 0x%p 0x%x\n", base, size);
		spin_unlock(&mm->vmm_area_lock);
		return -EINVAL;
	}

	list_del(&va->list);

	if (old)
		add_free_vmm_area(mm, old);
	if (old1)
		add_free_vmm_area(mm, old1);
	if (new)
		add_used_vmm_area(mm, new);

	spin_unlock(&mm->vmm_area_lock);

	return 0;
}

static void dump_vmm_areas(struct mm_struct *mm)
{
	struct vmm_area *va;

	pr_info("***** free vmm areas *****\n");
	list_for_each_entry(va, &mm->vmm_area_free, list) {
		pr_info("vmm_area: start:0x%p ---> 0x%p 0x%p\n",
				va->start, va->end, va->size);
	}

	pr_info("***** used vmm areas *****\n");
	list_for_each_entry(va, &mm->vmm_area_used, list) {
		pr_info("vmm_area: start:0x%p ---> 0x%p 0x%p\n",
				va->start, va->end, va->size);
	}
}

static void inline release_vmm_area_pg(struct vmm_area *va)
{
	struct page *page = va->p_head, *tmp;

	while (page != NULL) {
		tmp = page->next;
		release_pages(page);
		page = tmp;
	}
}

static void inline release_vmm_area_bk(struct vmm_area *va)
{
	struct mem_block *block, *n;

	list_for_each_entry_safe(block, n, &va->b_head, list) {
		release_mem_block(block);
		list_del(&block->list);
	}
}

static void release_vmm_area_in_vm0(struct vm *vm)
{
	struct vm *vm0 = get_vm_by_id(0);
	struct mm_struct *mm = &vm0->mm;
	struct vmm_area *va, *n;

	spin_lock(&mm->vmm_area_lock);

	list_for_each_entry_safe(va, n, &mm->vmm_area_used, list) {
		if (va->vmid != vm->vmid)
			continue;

		list_del(&va->list);
		add_free_vmm_area(mm, va);
	}

	spin_unlock(&mm->vmm_area_lock);
}

void release_vm_memory(struct vm *vm)
{
	unsigned long type;
	struct mm_struct *mm;
	struct page *page, *tmp;
	struct vmm_area *va, *n;

	if (!vm)
		return;

	mm = &vm->mm;
	page = mm->page_head;

	/*
	 * - release all the vmm_area and its memory
	 * - release the page table page and page table
	 * - set all the mm_struct to 0
	 * this function will not be called when vm is
	 * running, do not to require the lock
	 */
	list_for_each_entry_safe(va, n, &mm->vmm_area_used, list) {
		type = va->flags & VM_MAP_TYPE_MASK;
		if ((type == VM_PT) || (type == 0))
			continue;

		switch (type) {
		case VM_LN:
			free((void *)va->pstart);
			break;
		case VM_BK:
			release_vmm_area_bk(va);
			break;
		case VM_PG:
			release_vmm_area_pg(va);
			break;
		default:
			break;
		}

		list_del(&va->list);
		free(va);
	}

	list_for_each_entry_safe(va, n, &mm->vmm_area_free, list) {
		list_del(&va->list);
		free(va);
	}

	/* release the vm0's memory belong to this vm */
	release_vmm_area_in_vm0(vm);

	while (page != NULL) {
		tmp = page->next;
		release_pages(page);
		page = tmp;
	}

	free_pages((void *)mm->pgd_base);
	memset(mm, 0, sizeof(struct mm_struct));
}

unsigned long create_hvm_iomem_map(struct vm *vm,
			unsigned long phy, uint32_t size)
{
	struct vmm_area *va;
	struct vm *vm0 = get_vm_by_id(0);

	va = alloc_free_vmm_area(&vm0->mm, size, PAGE_MASK, 0);
	if (!va)
		return 0;

	va->vmid = vm->vmid;
	va->flags |= (VM_PT | VM_IO);
	va->pstart = phy;
	map_vmm_area(&vm0->mm, va, 0);

	return va->start;
}

void destroy_hvm_iomem_map(unsigned long vir, uint32_t size)
{
	struct vm *vm0 = get_vm_by_id(0);

	/* just destroy the vm0's mapping entry */
	size = PAGE_BALIGN(size);
	destroy_guest_mapping(&vm0->mm, vir, size);
}

/*
 * map VMx virtual memory to hypervisor memory
 * space to let hypervisor can access guest vm's
 * memory
 */
void *map_vm_mem(unsigned long gva, size_t size)
{
	unsigned long pa;

	/* assume the memory is continuously */
	pa = guest_va_to_pa(gva, 1);
	if (create_host_mapping(pa, pa, size, 0))
		return NULL;

	return (void *)pa;
}

void unmap_vm_mem(unsigned long gva, size_t size)
{
	unsigned long pa;

	/*
	 * what will happend if this 4k mapping is used
	 * in otherwhere
	 */
	pa = guest_va_to_pa(gva, 1);
	destroy_host_mapping(pa, size);
}

static int __vm_mmap(struct mm_struct *mm, unsigned long hvm_mmap_base,
		unsigned long offset, unsigned long size)
{
	unsigned long vir, phy, value;
	unsigned long *vm_pmd, *vm0_pmd;
	uint64_t attr;
	int i, vir_off, phy_off, count, left;
	struct vm *vm0 = get_vm_by_id(0);
	struct mm_struct *mm0 = &vm0->mm;

	offset = ALIGN(offset, PMD_MAP_SIZE);
	size = BALIGN(size, PMD_MAP_SIZE);
	vir = offset;
	phy = hvm_mmap_base;

	left = size >> PMD_RANGE_OFFSET;
	phy_off = pmd_idx(phy);
	vm0_pmd = (unsigned long *)alloc_guest_pmd(mm0, phy);
	if (!vm0_pmd)
		return -ENOMEM;

	attr = page_table_description(VM_DES_BLOCK | VM_NORMAL);

	while (left > 0) {
		vm_pmd = (unsigned long *)get_mapping_pmd(mm->pgd_base, vir, 0);
		if (mapping_error(vm_pmd))
			return -EIO;

		vir_off = pmd_idx(vir);
		count = (PAGE_MAPPING_COUNT - vir_off);
		count = count > left ? left : count;

		for (i = 0; i < count; i++) {
			value = *(vm_pmd + vir_off);
			value &= PAGETABLE_ATTR_MASK;
			value |= attr;

			*(vm0_pmd + phy_off) = value;

			vir += PMD_MAP_SIZE;
			phy += PMD_MAP_SIZE;
			vir_off++;
			phy_off++;

			if ((phy_off & (PAGE_MAPPING_COUNT - 1)) == 0) {
				phy_off = 0;
				vm0_pmd = (unsigned long *)alloc_guest_pmd(mm0, phy);
				if (!vm0_pmd)
					return -ENOMEM;
			}
		}

		left -= count;
	}

	/* this function always run in vm0 */
	flush_local_tlb_guest();
	flush_icache_all();

	return 0;
}

/*
 * map the guest vm memory space to vm0 to let vm0 can access
 * the memory space of the guest VM, this function can only
 * map the normal memory for the guest VM, will not map IO
 * memory
 *
 * offset - the base address need to be mapped
 * size - the size need to mapped
 */
struct vmm_area *vm_mmap(struct vm *vm, unsigned long offset, size_t size)
{
	struct vmm_area *va;
	struct vm *vm0 = get_vm_by_id(0);

	va = alloc_free_vmm_area(&vm0->mm, size, BLOCK_MASK,
			VM_NORMAL | VM_PT);
	if (!va)
		return 0;

	if (__vm_mmap(&vm->mm, va->start, offset, size)) {
		pr_err("map guest vm memory to vm0 failed\n");
		return 0;
	}

	/* mark this vmm_area is for guest vm map */
	va->vmid = vm->vmid;

	return va;
}

static int __vm_unmmap(struct mm_struct *mm0, struct vmm_area *va)
{
	unsigned long *vm0_pmd;
	int left, count, offset;
	unsigned long phy = va->start;

	left = va->size >> PMD_RANGE_OFFSET;

	/* TBD */
	spin_lock(&mm0->mm_lock);

	while (left > 0) {
		vm0_pmd = (unsigned long *)get_mapping_pmd(mm0->pgd_base, phy, 0);
		if (mapping_error(vm0_pmd))
			return -EFAULT;

		offset = pmd_idx(phy);
		count = PAGE_MAPPING_COUNT - offset;
		if (count > left)
			count = left;

		memset((void *)(vm0_pmd + offset), 0,
				count * sizeof(unsigned long));

		if ((offset == 0) && (count == PAGE_MAPPING_COUNT)) {
			/* here we can free this pmd page TBD */
		}

		phy += count << PMD_RANGE_OFFSET;
		left -= count;
	}

	spin_unlock(&mm0->mm_lock);

	flush_local_tlb_guest();

	return 0;
}

/*
 * unmap the guest VM memory from the VM0, only release
 * the Normal memory
 */
void vm_unmmap(struct vm *vm)
{
	int ret;
	struct vm *vm0 = get_vm_by_id(0);
	struct mm_struct *mm = &vm0->mm;
	struct vmm_area *va, *n;

	spin_lock(&mm->vmm_area_lock);

	list_for_each_entry_safe(va, n, &mm->vmm_area_used, list) {
		if ((va->flags & VM_NORMAL) && (va->vmid == vm->vmid) &&
				(va->flags & VM_PT)) {
			ret = __vm_unmmap(&vm0->mm, va);
			if (ret)
				pr_err("unmap 0x%p @0x%p failed for vm-%d\n", va->start,
						va->size, vm->vmid);

			list_del(&va->list);
			add_free_vmm_area(mm, va);
		}
	}

	spin_unlock(&mm->vmm_area_lock);
}

static int __alloc_vm_memory(struct mm_struct *mm, struct vmm_area *va)
{
	int i, count;
	unsigned long base;
	struct mem_block *block;

	base = ALIGN(va->start, MEM_BLOCK_SIZE);
	if (base != va->start) {
		pr_warn("memory base is not mem_block align\n");
		return -EINVAL;
	}

	init_list(&va->b_head);
	va->flags |= VM_BK;
	count = va->size >> MEM_BLOCK_SHIFT;

	/*
	 * here get all the memory block for the vm
	 * TBD: get contiueous memory or not contiueous ?
	 */
	for (i = 0; i < count; i++) {
		block = alloc_mem_block(GFB_VM);
		if (!block)
			return -ENOMEM;

		list_add_tail(&va->b_head, &block->list);
	}

	return 0;
}

int alloc_vm_memory(struct vm *vm)
{
	struct mm_struct *mm = &vm->mm;
	struct vmm_area *va;

	list_for_each_entry(va, &mm->vmm_area_used, list) {
		if (!(va->flags & VM_NORMAL))
			continue;

		if (__alloc_vm_memory(mm, va))
			goto out;

		if (map_vmm_area(mm, va, 0))
			goto out;
	}

	return 0;

out:
	pr_err("alloc memory for vm-%d failed\n", vm->vmid);
	release_vm_memory(vm);
	return -ENOMEM;
}

phy_addr_t translate_vm_address(struct vm *vm, unsigned long a)
{
	return mmu_translate_guest_address((void *)vm->mm.pgd_base, a);
}

static void vmm_area_init(struct mm_struct *mm, int bit64)
{
	unsigned long base, size;

	init_list(&mm->vmm_area_free);
	init_list(&mm->vmm_area_used);

	/*
	 * the virtual memory space for a virtual machine:
	 * 64bit - 48bit virtual address
	 * 32bit - 32bit virutal address (Without LPAE)
	 * 32bit - TBD (with LPAE)
	 */
	if (bit64) {
		base = 0x0;
		size = 0x0001000000000000;
	} else {
#ifdef CONFIG_VM_LPAE
		base = 0x0;
		size = 0x100000000;
#else
		base = 0x0;
		size = 0x100000000;
#endif
	}

	create_free_vmm_area(mm, base, size, 0);
}

void vm_mm_struct_init(struct vm *vm)
{
	struct mm_struct *mm = &vm->mm;

	mm->page_head = NULL;
	mm->pgd_base = 0;
	mm->vm = vm;
	spin_lock_init(&mm->mm_lock);
	spin_lock_init(&mm->vmm_area_lock);
	init_list(&mm->vmm_area_free);
	init_list(&mm->vmm_area_used);

	mm->pgd_base = alloc_pgd();
	if (mm->pgd_base == 0) {
		pr_err("No memory for vm page table\n");
		return;
	}

	if (vm_is_64bit(vm))
		vmm_area_init(mm, 1);
	else
		vmm_area_init(mm, 0);
}

void vm_init_shmem(struct vm *vm, uint64_t base, uint64_t size)
{

}

void *vm_map_shmem(struct vm *vm, void *phy, uint32_t size,
		unsigned long flags)
{
	return NULL;
}

int vm_mm_init(struct vm *vm)
{
	int ret;
	struct vmm_area *va;
	struct mm_struct *mm = &vm->mm;

	dump_vmm_areas(&vm->mm);

	/* just mapping the physical memory for native VM */
	list_for_each_entry(va, &mm->vmm_area_used, list) {
		if (!(va->flags & VM_NORMAL))
			continue;

		ret = map_vmm_area(mm, va, 0);
		if (ret) {
			pr_err("build mem ma failed for vm-%d 0x%p 0x%p\n",
				vm->vmid, va->start, va->size);
		}
	}

	return 0;
}
