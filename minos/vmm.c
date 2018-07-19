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

	pr_info("register memory region: 0x%x 0x%x %d %d %s\n",
			res->mem_base, res->mem_end, res->type,
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


	page = alloc_page();
	if (!page)
		return NULL;

	if (vm) {
		mm = &vm->mm;
		spin_lock(&mm->lock);
		page->next = mm->head;
		mm->head = page;
		spin_unlock(&mm->lock);
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

int vm_mm_init(struct vm *vm)
{
	struct memory_region *region;
	struct mm_struct *mm = &vm->mm;
	struct list_head *list = mem_list.next;
	struct list_head *head = &mem_list;

	init_list(&mm->mem_list);
	init_list(&mm->block_list);
	mm->head = NULL;
	mm->page_table_base = 0;
	spin_lock_init(&mm->lock);

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

	flush_all_tlb();

	return 0;
}
