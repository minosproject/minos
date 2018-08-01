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
#include <minos/virt.h>
#include <minos/vm.h>
#include <minos/vcpu.h>
#include <minos/mmu.h>

extern unsigned char __el2_ttb0_pgd;
extern unsigned char __el2_ttb0_pud;
extern unsigned char __el2_ttb0_pmd_code;
extern unsigned char __el2_ttb0_pmd_io;

static unsigned long el2_ttb0_pgd;

LIST_HEAD(mem_list);
static LIST_HEAD(shared_mem_list);
static DEFINE_SPIN_LOCK(host_lock);

static unsigned long *vm0_mmap_base;

int register_memory_region(struct memtag *res)
{
	struct memory_region *region;

	if (res == NULL)
		return -EINVAL;

	if (!res->enable)
		return -EAGAIN;

	/* only parse the memory for guest */
	if (res->vmid == VMID_HOST)
		return -EINVAL;

	region = (struct memory_region *)
		malloc(sizeof(struct memory_region));
	if (!region) {
		pr_error("No memory for new memory region\n");
		return -ENOMEM;
	}

	pr_info("MEM : 0x%x 0x%x %d %d %s\n", res->mem_base,
			res->mem_end, res->type,
			res->vmid, res->name);

	memset((char *)region, 0, sizeof(struct memory_region));

	/*
	 * minos no using phy --> phy mapping
	 */
	region->vir_base = res->mem_base;
	region->mem_base = res->mem_base;
	region->size = res->mem_end - res->mem_base + 1;
	region->vmid = res->vmid;

	init_list(&region->list);

	/*
	 * shared memory is for all vm to ipc purpose
	 */
	if (res->type == 0x2) {
		region->type = MEM_TYPE_NORMAL;
		list_add(&shared_mem_list, &region->list);
	} else {
		if (res->type == 0x0)
			region->type = MEM_TYPE_NORMAL;
		else
			region->type = MEM_TYPE_IO;

		list_add_tail(&mem_list, &region->list);
	}

	return 0;
}

static unsigned long alloc_guest_page_table(void)
{
	/*
	 * return the table base address, this function
	 * is called when init the vm
	 *
	 * 2 pages for each VM to map 1T IPA memory
	 *
	 */
	void *page;

	page = __get_free_pages(GUEST_PGD_PAGE_NR, GUEST_PGD_PAGE_ALIGN);
	if (!page)
		panic("No memory to map vm memory\n");

	memset(page, 0, SIZE_4K * GUEST_PGD_PAGE_NR);
	return (unsigned long)page;
}

void *get_host_translation_page(int pages, void *data)
{
	return get_free_pages(pages);
}

void *get_guest_translation_page(int pages, void *data)
{
	struct vm *vm = (struct vm *)data;
	struct page *page = NULL;
	struct mm_struct *mm = NULL;

	/*
	 * this function will only be excuted in
	 * map_vm_memory, map_vm_memory has alreadly
	 * get the spin lock, so do not get it here
	 */
	page = alloc_pages(pages);
	if (!page)
		return NULL;

	if (vm) {
		mm = &vm->mm;
		page->next = mm->head;
		mm->head = page;
	}

	return page_to_addr(page);
}

int create_host_mapping(unsigned long vir,
		unsigned long phy, size_t size, int type)
{
	int ret = 0;
	unsigned long vir_base, phy_base, tmp;
	struct mapping_struct map_info;

	/*
	 * for host mapping, IO and Normal memory all mapped
	 * as MEM_BLOCK_SIZE ALIGN
	 */
	vir_base = ALIGN(vir, MEM_BLOCK_SIZE);
	phy_base = ALIGN(phy, MEM_BLOCK_SIZE);
	tmp = BALIGN(vir_base + size, MEM_BLOCK_SIZE);
	size = tmp - vir_base;

	memset(&map_info, 0, sizeof(struct mapping_struct));
	map_info.table_base = el2_ttb0_pgd;
	map_info.vir_base = vir_base;
	map_info.phy_base = phy_base;
	map_info.size = size;
	map_info.lvl = PGD;
	map_info.host = 1;
	map_info.data = NULL;
	map_info.mem_type = type;
	map_info.get_free_pages = get_host_translation_page;

	spin_lock(&host_lock);
	ret = create_mem_mapping(&map_info);
	spin_unlock(&host_lock);

	return ret;
}

int destroy_host_mapping(unsigned long vir, size_t size, int type)
{
	unsigned long end;
	struct mapping_struct map_info;

	end = vir + size;
	end = BALIGN(end, PAGE_SIZE);
	vir = ALIGN(vir, PAGE_SIZE);
	size = end - vir;

	memset(&map_info, 0, sizeof(struct mapping_struct));
	map_info.table_base = el2_ttb0_pgd;
	map_info.vir_base = vir;
	map_info.size = size;
	map_info.lvl = PGD;
	map_info.host = 1;
	map_info.mem_type = type;
	map_info.data = NULL;

	return destroy_mem_mapping(&map_info);
}

static int create_guest_mapping(struct vm *vm, unsigned long tbase,
			unsigned long vir, unsigned long phy,
			size_t size, int type)
{
	unsigned long vir_base, phy_base, tmp;
	struct mapping_struct map_info;

	vir_base = ALIGN(vir, SIZE_4K);
	phy_base = ALIGN(phy, SIZE_4K);
	tmp = BALIGN(vir_base + size, SIZE_4K);
	size = tmp - vir_base;

	memset(&map_info, 0, sizeof(struct mapping_struct));
	map_info.table_base = tbase;
	map_info.vir_base = vir_base;
	map_info.phy_base = phy_base;
	map_info.size = size;
	map_info.lvl = PUD;
	map_info.host = 0;
	map_info.data = (void *)vm;
	map_info.mem_type = type;
	map_info.get_free_pages = get_host_translation_page;

	return create_mem_mapping(&map_info);
}

static int destroy_guest_mapping(struct vm *vm, unsigned long tt,
		unsigned long vir, size_t size, int type)
{
	unsigned long end;
	struct mapping_struct map_info;

	end = vir + size;
	end = BALIGN(end, PAGE_SIZE);
	vir = ALIGN(vir, PAGE_SIZE);
	size = end - vir;

	memset(&map_info, 0, sizeof(struct mapping_struct));
	map_info.table_base = tt;
	map_info.vir_base = vir;
	map_info.size = size;
	map_info.lvl = PUD;
	map_info.host = 0;
	map_info.mem_type = type;
	map_info.data = (void *)vm;

	return destroy_mem_mapping(&map_info);
}

int map_vm_memory(struct vm *vm, unsigned long vir_base,
		unsigned long phy_base, size_t size, int type)
{
	int ret;
	struct mm_struct *mm = &vm->mm;

	spin_lock(&mm->lock);
	ret = create_guest_mapping(vm, mm->page_table_base,
			vir_base, phy_base, size, type);
	spin_unlock(&mm->lock);

	return ret;
}

int unmap_vm_memory(struct vm *vm, unsigned long vir_addr,
			size_t size, int type)
{
	int ret;
	struct mm_struct *mm = &vm->mm;

	spin_lock(&mm->lock);
	ret = destroy_guest_mapping(vm, mm->page_table_base,
			vir_addr, size, type);
	spin_unlock(&mm->lock);

	return ret;
}

void release_vm_memory(struct vm *vm)
{
	struct mem_block *block, *n;
	struct mm_struct *mm;
	struct page *page, *tmp;

	if (!vm)
		return;

	mm = &vm->mm;
	page = mm->head;

	/*
	 * - release the block list
	 * - release the page table page and page table
	 * - set all the mm_struct to 0
	 * this function will not be called when vm is
	 * running, do not to require the lock
	 */
	list_for_each_entry_safe(block, n, &mm->block_list, list)
		release_mem_block(block);

	while (page != NULL) {
		tmp = page->next;
		release_pages(page);
		page = tmp;
	}

	free_pages((void *)mm->page_table_base);
	memset(mm, 0, sizeof(struct mm_struct));
}

static inline unsigned long
get_vm_mapping_pte(struct vm *vm, unsigned long vir)
{
	return get_mapping_pte(vm->mm.page_table_base, vir, 0);
}

static inline unsigned long
get_vm_mapping_pmd(struct vm *vm, unsigned long vir)
{
	return get_mapping_pmd(vm->mm.page_table_base, vir, 0);
}

static inline unsigned long
get_vm_mapping_pud(struct vm *vm, unsigned long vir)
{
	return get_mapping_pud(vm->mm.page_table_base, vir, 0);
}

static inline unsigned long
get_vm_mapping_pgd(struct vm *vm, unsigned long vir)
{
	return get_mapping_pgd(vm->mm.page_table_base, vir, 0);
}

static inline unsigned long get_host_mapping_pte(unsigned long vir)
{
	return get_mapping_pte(el2_ttb0_pgd, vir, 1);
}

static inline unsigned long get_host_mapping_pmd(unsigned long vir)
{

	return get_mapping_pmd(el2_ttb0_pgd, vir, 1);
}

static inline unsigned long get_host_mapping_pud(unsigned long vir)
{

	return get_mapping_pud(el2_ttb0_pgd, vir, 1);
}

static inline unsigned long get_host_mapping_pgd(unsigned long vir)
{
	return get_mapping_pgd(el2_ttb0_pgd, vir, 1);
}

unsigned long get_vm_mmap_info(int vmid, unsigned long *size)
{
	unsigned long vir;

	vir = VM0_MMAP_REGION_START + (vmid * VM_MMAP_MAX_SIZE);
	*size = VM_MMAP_MAX_SIZE;

	return vir;
}

int vm_mmap(struct vm *vm, unsigned long o, unsigned long s)
{
	unsigned long vir;
	unsigned long *vm_pmd;
	struct mm_struct *mm = &vm->mm;
	int start = 0, i, off, count;
	unsigned long offset = o, value;
	unsigned long size = s;
	uint64_t attr;

	offset = ALIGN(offset, MEM_BLOCK_SIZE);
	size = BALIGN(size, MEM_BLOCK_SIZE);
	vir = mm->mem_base + offset;

	if (size > VM_MMAP_MAX_SIZE)
		size = VM_MMAP_MAX_SIZE;

	count = size >> MEM_BLOCK_SHIFT;
	start = VM_MMAP_ENTRY_COUNT * vm->vmid;

	/* clear the previous map */
	memset(vm0_mmap_base + start, 0, VM_MMAP_ENTRY_COUNT);

	vm_pmd = (unsigned long *)get_vm_mapping_pmd(vm, vir);
	if (mapping_error(vm_pmd))
		return -EIO;

	off = (vir - ALIGN(vir, PUD_MAP_SIZE)) >> MEM_BLOCK_SHIFT;

	/*
	 * map the memory as a IO memory in guest to
	 * avoid the cache issue
	 */
	attr = get_tt_description(0, MEM_TYPE_NORMAL, DESCRIPTION_BLOCK);

	for (i = 0; i < count; i++) {
		value = *(vm_pmd + off + i);
		value &= PAGETABLE_ATTR_MASK;
		value |= attr;
		*(vm0_mmap_base + start + i) = value;
	}

	flush_dcache_range((unsigned long)(vm0_mmap_base + start),
			sizeof(unsigned long) * count);
	flush_local_tlb_guest();

	return 0;
}

void vm_unmmap(struct vm *vm)
{
	int offset, i;

	offset = VM_MMAP_ENTRY_COUNT * vm->vmid;

	for (i = 0; i < VM_MMAP_ENTRY_COUNT; i++)
		*(vm0_mmap_base + offset + i) = 0;

	flush_dcache_range((unsigned long)(vm0_mmap_base + offset),
			sizeof(unsigned long) * VM_MMAP_ENTRY_COUNT);
	flush_local_tlb_guest();
}

int alloc_vm_memory(struct vm *vm, unsigned long start, size_t size)
{
	int i, count;
	unsigned long base;
	struct mm_struct *mm = &vm->mm;
	struct mem_block *block;

	base = ALIGN(start, MEM_BLOCK_SIZE);
	if (base != start)
		pr_warn("memory base is not mem_block align\n");

	/*
	 * first allocate the page table for vm, since
	 * the vm is not running, do not need to get
	 * the spin lock
	 */
	mm->page_table_base = alloc_guest_page_table();
	if (!mm->page_table_base)
		return -ENOMEM;

	mm->mem_base = base;
	mm->mem_size = size;
	mm->mem_free = size;
	count = size >> MEM_BLOCK_SHIFT;

	/*
	 * here get all the memory block for the vm
	 * TBD: get contiueous memory or not contiueous ?
	 */
	for (i = 0; i < count; i++) {
		block = alloc_mem_block(GFB_VM);
		if (!block)
			goto free_vm_memory;

		block->vmid = vm->vmid;
		list_add_tail(&mm->block_list, &block->list);
	}

	/* begin to map the memory */
	list_for_each_entry(block, &mm->block_list, list) {
		i = map_vm_memory(vm, base, block->phy_base,
				MEM_BLOCK_SIZE, MEM_TYPE_NORMAL);
		if (i)
			goto free_vm_memory;

		base += MEM_BLOCK_SIZE;
	}

	return 0;

free_vm_memory:
	release_vm_memory(vm);

	return -ENOMEM;
}

void vm_mm_struct_init(struct vm *vm)
{
	struct mm_struct *mm = &vm->mm;

	init_list(&mm->mem_list);
	init_list(&mm->block_list);
	mm->head = NULL;
	mm->page_table_base = 0;
	spin_lock_init(&mm->lock);
}

int vm_mm_init(struct vm *vm)
{
	struct memory_region *region;
	struct mm_struct *mm = &vm->mm;
	struct list_head *list = mem_list.next;
	struct list_head *head = &mem_list;

	vm_mm_struct_init(vm);

	/*
	 * this function will excuted at bootup
	 * stage, so do not to aquire lock or
	 * disable irq
	 */
	while (list != head) {
		region = list_entry(list, struct memory_region, list);
		list = list->next;

		/* put this memory region to related vm */
		if (region->vmid == vm->vmid) {
			list_del(&region->list);
			list_add_tail(&mm->mem_list, &region->list);
		}
	}

	mm->page_table_base = alloc_guest_page_table();
	if (mm->page_table_base == 0) {
		pr_error("No memory for vm page table\n");
		return -ENOMEM;
	}

	if (is_list_empty(&mm->mem_list)) {
		pr_error("No memory config for this vm\n");
		return -EINVAL;
	}

	list_for_each_entry(region, &mm->mem_list, list) {
		create_guest_mapping(vm, mm->page_table_base, region->mem_base,
				region->vir_base, region->size, region->type);
	}

	list_for_each_entry(region, &shared_mem_list, list) {
		create_guest_mapping(vm, mm->page_table_base, region->mem_base,
				region->vir_base, region->size, region->type);
	}

	return 0;
}

int vmm_init(void)
{
	struct vm *vm;
	struct mm_struct *mm;
	struct memory_region *region;
	unsigned long pud;
	unsigned long *tbase;

	/* map all vm0 dram memory to host */
	vm = get_vm_by_id(0);
	if (!vm)
		panic("no vm found for vmid 0\n");

	mm = &vm->mm;
	list_for_each_entry(region, &mm->mem_list, list) {
		if (region->type != MEM_TYPE_NORMAL)
			continue;

		create_host_mapping(region->vir_base, region->mem_base,
				region->size, region->type);
	}

	/*
	 * 0xc0000000 - 0xffffffff of vm0 (1G space) is used
	 * to mmap the other vm's memory to vm0, 1G space spilt
	 * n region, one vm has a region, so if the system has
	 * max 64 vms, then each vm can mmap 16M max one time
	 */
	pud = get_vm_mapping_pud(vm, VM0_MMAP_REGION_START);
	if (pud > INVALID_MAPPING)
		panic("mmap region should not mapped for vm0\n");

	pud = (unsigned long)get_free_page();
	if (!pud)
		panic("no more memory\n");

	memset((void *)pud, 0, PAGE_SIZE);
	tbase = (unsigned long *)mm->page_table_base;
	tbase += VM0_MMAP_REGION_START >> PUD_RANGE_OFFSET;
	create_guest_level_mapping(PUD, (unsigned long)tbase,
			pud, DESCRIPTION_TABLE);
	vm0_mmap_base = (unsigned long *)pud;

	return 0;
}

static int vmm_early_init(void)
{
	el2_ttb0_pgd = (unsigned long)&__el2_ttb0_pgd;

	return 0;
}

early_initcall(vmm_early_init);
