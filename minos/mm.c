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

static struct mem_allocator *allocator = NULL;
extern struct mem_allocator dynamic_allocator;
extern struct mem_allocator static_allocator;

void *malloc(size_t size)
{
	return allocator->malloc(size);
}

void *zalloc(size_t size)
{
	char *base;

	size = BALIGN(size, sizeof(unsigned long));
	base = allocator->malloc(size);
	if (!base)
		return NULL;

	memset(base, 0, size);

	return base;
}

void *get_free_pages_align(int pages, int align)
{
	return allocator->get_free_pages_align(pages, align);
}

void free(void *addr)
{
	allocator->free(addr);
}

void free_pages(void *addr)
{
	allocator->free_pages(addr);
}

struct mem_block *get_mem_block(unsigned long flags)
{
	return allocator->get_mem_block(flags);
}

void free_mem_block(struct mem_block *block)
{
	allocator->free_mem_block(block);
}

int mm_init(void)
{
#ifdef CONFIG_MINOS_MEM_ALLOCATOR_STATIC
	allocator = &static_allocator;
#else
	allocator = &dynamic_allocator;
#endif
	pr_info("minos mm allocator using %s\n", allocator->name);

	return allocator->init();
}
