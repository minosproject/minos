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

void *bootmem_start;
void *bootmem_end;
unsigned long bootmem_size = 0;
static DEFINE_SPIN_LOCK(bootmem_lock);
void *bootmem_page_base;

#define BOOTMEM_MIN_SIZE	(128 * 1024)

void *alloc_boot_mem(size_t size)
{
	size_t request_size;
	void *base = NULL;

	request_size = BALIGN(size, sizeof(unsigned long));

	spin_lock(&bootmem_lock);
	if (bootmem_size < request_size)
		panic("no more memory or mm has been init\n");
	else
		base = bootmem_start;

	bootmem_start += request_size;
	bootmem_size -= request_size;
	spin_unlock(&bootmem_lock);

	return base;
}

void *alloc_boot_pages(int pages)
{
	void *base = NULL;
	size_t request_size = pages * PAGE_SIZE;

	if (pages <= 0)
		return NULL;

	spin_lock(&bootmem_lock);
	if (bootmem_size < request_size)
		panic("no more memory or mm has been init\n");

	if (bootmem_page_base -
			request_size < bootmem_start)
		panic("no more memory or mm has been init\n");

	base = bootmem_page_base - request_size;
	bootmem_page_base = base;
	bootmem_size -= request_size;
	spin_unlock(&bootmem_lock);

	return base;
}

void bootmem_init(void)
{
	size_t min_size = 0, size;
	extern unsigned char __code_end;
	extern unsigned char __code_start;

#ifdef CONFIG_BOOTMEM_SIZE
	min_size = CONFIG_BOOTMEM_SIZE;
#else
	min_size = BOOTMEM_MIN_SIZE;
#endif

	if (min_size < BOOTMEM_MIN_SIZE)
		min_size = BOOTMEM_MIN_SIZE;

	size = (&__code_end) - (&__code_start);
	size = PAGE_BALIGN(size);

	if ((size + min_size) >= CONFIG_MINOS_RAM_SIZE)
		panic("no more memory for bootmem\n");

	size = CONFIG_MINOS_RAM_SIZE - size;
	min_size = min_size > size ? size : min_size;

	bootmem_start = (void *)BALIGN((unsigned long)&__code_end,
			sizeof(unsigned long));
	bootmem_end = (void *)PAGE_BALIGN((unsigned long)&__code_end
			+ min_size);
	bootmem_size = bootmem_end - bootmem_start;
	bootmem_page_base = bootmem_end;

	pr_info("bootmem start-0x%p end-0x%p size-0x%x\n",
			(unsigned long)bootmem_start,
			(unsigned long)bootmem_end,
			(unsigned long)bootmem_size);
}
