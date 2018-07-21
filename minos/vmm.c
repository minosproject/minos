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

static LIST_HEAD(shared_mem_list);
LIST_HEAD(mem_list);

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

void *get_vm_translation_page(struct vm *vm)
{
	struct page *page = NULL;
	struct mm_struct *mm = NULL;

	/*
	 * this function will only be excuted in
	 * map_vm_memory, map_vm_memory has alreadly
	 * get the spin lock, so do not get it here
	 */
	page = alloc_page();
	if (!page)
		return NULL;

	if (vm) {
		mm = &vm->mm;
		page->next = mm->head;
		mm->head = page;
	}

	return page_to_addr(page);
}

int map_vm_memory(struct vm *vm, unsigned long vir_base,
		unsigned long phy_base, size_t size, int type)
{
	int ret;
	struct mm_struct *mm = &vm->mm;

	spin_lock(&mm->lock);
	ret = map_guest_memory(vm, mm->page_table_base,
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
	ret = unmap_guest_memory(vm, mm->page_table_base,
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
		map_guest_memory(vm, mm->page_table_base, region->mem_base,
				region->vir_base,
				region->size, region->type);
	}

	list_for_each_entry(region, &shared_mem_list, list) {
		map_guest_memory(vm, mm->page_table_base, region->mem_base,
				region->vir_base,
				region->size, region->type);
	}

	return 0;
}
