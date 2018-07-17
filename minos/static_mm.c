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

#include <minos/types.h>
#include <config/config.h>
#include <minos/spinlock.h>
#include <minos/minos.h>
#include <minos/init.h>
#include <minos/mmu.h>
#include <minos/mm.h>

extern unsigned char __code_start;
extern unsigned char __code_end;

static char *free_mem_base = NULL;
static unsigned long free_mem_size = 0;
static spinlock_t mem_block_lock;
static char *free_4k_base = 0;

static int static_mm_init(void)
{
	size_t size;

	spin_lock_init(&mem_block_lock);
	size = (&__code_end) - (&__code_start);
	size = BALIGN(size, sizeof(unsigned long));
	free_mem_base = (char *)(CONFIG_MINOS_START_ADDRESS + size);
	free_mem_size = CONFIG_MINOS_RAM_SIZE - size;

	/*
	 * assume the memory region is 4k align
	 */
	free_4k_base = free_mem_base + free_mem_size;

	return 0;
}

static void *static_malloc(size_t size)
{
	size_t request_size;
	char *base;

	request_size = BALIGN(size, sizeof(unsigned long));

	spin_lock(&mem_block_lock);

	if (free_mem_size < request_size)
		base = NULL;
	else
		base = free_mem_base;

	free_mem_base += request_size;
	free_mem_size -= request_size;

	spin_unlock(&mem_block_lock);

	return base;
}

static void *static_get_free_pages(int pages, int mask)
{
	size_t request_size = pages * SIZE_4K;
	char *base;

	if (pages <= 0)
		return NULL;

	spin_lock(&mem_block_lock);
	if (free_mem_size < request_size)
		return NULL;

	if (((unsigned long)free_4k_base - request_size) < free_mem_size)
		return NULL;

	base = free_4k_base - request_size;
	free_4k_base = base;
	free_mem_size -= request_size;
	spin_unlock(&mem_block_lock);

	return base;
}

void static_free(void *addr)
{
	pr_warn("free not support on static mode\n");
}

void static_free_pages(void *addr)
{
	pr_warn("free_pages not supported on static mode\n");
}

static struct mem_block *static_alloc_mem_block(unsigned long flags)
{
	return NULL;
}

static void static_free_mem_block(struct mem_block *block)
{

}

struct mem_allocator static_allocator __used = {
	.name			= "static allocator",
	.init			= static_mm_init,
	.malloc			= static_malloc,
	.get_free_pages_align 	= static_get_free_pages,
	.free			= static_free,
	.free_pages		= static_free_pages,
	.get_mem_block		= static_alloc_mem_block,
	.free_mem_block 	= static_free_mem_block,
};
