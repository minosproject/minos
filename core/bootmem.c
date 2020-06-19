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
#include <minos/mm.h>

static long bootmem_size;
static int bootmem_locked;

static struct slab_header *bootmem_slab_free;

int bootmem_is_locked(void)
{
	return bootmem_locked;
}

void reclaim_bootmem(void)
{
	static struct slab_header *sh;
	bootmem_locked = 1;

	/*
	 * check whether there are 1 page between bootmem_base
	 * and minos_stack_bottom
	 */
	if (minos_stack_bottom > minos_bootmem_base)
		add_slab_mem(minos_bootmem_base, minos_stack_bottom - minos_bootmem_base);

	while (bootmem_slab_free) {
		sh = bootmem_slab_free->next;
		add_slab_mem((unsigned long)bootmem_slab_free, bootmem_slab_free->size);
		bootmem_slab_free = sh;
	}
}

void *alloc_boot_pages(int pages)
{
	void *base = NULL;
	unsigned long end;
	struct slab_header *sh;
	size_t request_size = pages * PAGE_SIZE;

	if (bootmem_locked)
		panic("bootmem: allocator is locked\n");

	if (unlikely(pages <= 0))
		return NULL;

	if (request_size > bootmem_size)
		panic("bootmem: no more bootmem check the config\n");

	if (!IS_PAGE_ALIGN(minos_end)) {
		end = PAGE_BALIGN(minos_end);
		sh = (struct slab_header *)minos_end;
		sh->size = end - minos_end;

		sh->next = bootmem_slab_free;
		bootmem_slab_free = sh;

		minos_end = end;
	}

	base = (void *)minos_end;
	bootmem_size -= request_size;
	minos_end += request_size;

	return base;
}

static void *alloc_boot_mem_from_slab(size_t request_size)
{
	struct slab_header *tmp = bootmem_slab_free;
	struct slab_header *prev = NULL;
	struct slab_header *slab = NULL;
	struct slab_header *sh;
	size_t left_size;
	void *base;

	while (tmp) {
		if (tmp->size >= request_size) {
			slab = tmp;
			break;
		}

		prev = tmp;
		tmp = tmp->next;
	}

	if (!slab)
		return NULL;

	/*
	 * delete the slab header
	 */
	if (slab == bootmem_slab_free)
		bootmem_slab_free = slab->next;
	else
		prev->next = slab->next;

	/*
	 * allocate a new boot memory, and if the slab memory
	 * still have free memory, put the free memory to
	 * the slab free list for next time use
	 */
	base = (void *)slab;
	left_size = slab->size - request_size;

	if (left_size >= sizeof(struct slab_header)) {
		sh = (struct slab_header *)(base + request_size);
		sh->size = left_size;

		sh->next = bootmem_slab_free;
		bootmem_slab_free = sh;
	}

	return base;
}

/*
 * bootmem will not be freed once it has been allocated
 * bootmem also can alloc small slab and one page, and
 * bootmem will only used at the boot stage when the irq
 * and scheduler is disabled so there is no spin lock needed
 * when alloc bootmem
 */
void *alloc_boot_mem(size_t size)
{
	size_t request_size = BALIGN(size, sizeof(unsigned long));
	void *base = NULL;

	if (bootmem_locked)
		panic("bootmem: allocator is locked\n");

	/*
	 * find from the list
	 */
	base = alloc_boot_mem_from_slab(request_size);
	if (base)
		return base;

	base = (void *)minos_end;
	minos_end += request_size;
	bootmem_size -= request_size;

	return base;
}

void bootmem_init(void)
{
	bootmem_size = CONFIG_MINOS_RAM_SIZE - (minos_end - minos_start);

	if (!IS_PAGE_ALIGN(minos_end) || (bootmem_size <= 0))
		panic("bootmem: memory layout is wrong after boot\n");

	pr_notice("bootmem start at 0x%p\n", minos_end);
}
