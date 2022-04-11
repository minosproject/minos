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
#include <virt/vmm.h>
#include <minos/mm.h>
#include <minos/arch.h>

/*
 * one shmem_block rrepresent one mem_block which is 2M
 * so it has 512 pages.
 */
struct shmem_block {
	struct mem_block *mb;
	unsigned long phy_base;
	uint16_t id;
	uint16_t free_pages;
	uint16_t current_index;
	unsigned long bitmap[BITS_TO_LONGS(PAGES_IN_BLOCK)];
};

struct shmem {
	int pages;
	int flags;
	int id;
	uint32_t pfn;
	struct shmem *next;
};

#define MAX_SHMEM_ID 32

static struct shmem *shmem_head;
static uint16_t sbid;
static DEFINE_SPIN_LOCK(shmem_lock);

static struct shmem_block *shmem_blocks[MAX_SHMEM_ID];

static struct shmem_block *alloc_shmem_block(void)
{
	struct shmem_block *sb;

	sb = zalloc(sizeof(struct shmem_block));
	if (!sb) {
		pr_err("no memory for shmem block\n");
		return NULL;
	}

	sb->mb = vmm_alloc_memblock();
	if (sb->mb == NULL) {
		free(sb);
		return NULL;
	}

	sb->phy_base = BFN2PHY(sb->mb->bfn);
	sb->free_pages = PAGES_IN_BLOCK;
	
	/*
	 * mapping this memory block to the host address space.
	 */
	if (create_host_mapping(ptov(sb->phy_base), ULONG(sb->phy_base),
			MEM_BLOCK_SIZE, VM_NORMAL_NC | VM_RW | VM_HUGE)) {
		pr_err("mapping share memory failed\n");
		free(sb);
		vmm_free_memblock(sb->mb);
		return NULL;
	}

	sb->id = sbid++;
	shmem_blocks[sb->id] = sb;

	return sb;
}

static void *__alloc_shmem(struct shmem_block *sb, int pages, int flags)
{
	struct shmem *shmem;
	unsigned long addr;
	int bits;

	bits = bitmap_find_next_zero_area_align(sb->bitmap,
			PAGES_IN_BLOCK, 0, pages, 1);
	if (bits >= PAGES_IN_BLOCK)
		return NULL;

	shmem = malloc(sizeof(struct shmem));
	if (!shmem)
		return NULL;

	addr = sb->phy_base + pfn2phy(bits);
	shmem->id = sb->id;
	shmem->pfn = phy2pfn(addr);
	shmem->flags = flags;
	shmem->pages = pages;

	shmem->next = shmem_head;
	shmem_head = shmem;

	bitmap_set(sb->bitmap, bits, pages);

	return (void *)ptov(addr);
}

void *alloc_shmem(int pages)
{
	struct shmem_block *sb;
	void *addr = NULL;
	int i;

	if (pages > PAGES_IN_BLOCK) {
		pr_err("share memory should be below %d pages\n", PAGES_IN_BLOCK);
		return NULL;
	}

	spin_lock(&shmem_lock);
	for (i = 0; i < sbid; i++) {
		sb = shmem_blocks[i];
		if (sb->free_pages >= pages) {
			addr = __alloc_shmem(sb, pages, 0);
			if (addr)
				break;
		}
	}

	if (addr == NULL) {
		if (sbid >= MAX_SHMEM_ID) {
			pr_err("can not alloc more shmem block\n");
			goto out;
		}

		sb = alloc_shmem_block();
		if (!sb) {
			pr_err("no more memblock for shmem\n");
			goto out;
		}

		addr = __alloc_shmem(sb, pages, 0);
	}
out:
	spin_unlock(&shmem_lock);

	return addr;
}

static void free_shmem_in_block(struct shmem *shmem)
{
	struct shmem_block *sb;
	uint32_t pfn;

	ASSERT(shmem->id < MAX_SHMEM_ID);
	sb = shmem_blocks[shmem->id];
	pfn = phy2pfn(pfn2phy(shmem->pfn) - ULONG(sb->phy_base));
	bitmap_clear(sb->bitmap, pfn, shmem->pages);
	sb->free_pages += shmem->pages;
	free(shmem);
}

void free_shmem(void *addr)
{
	struct shmem *shmem, *tmp = NULL;

	ASSERT(IS_PAGE_ALIGN(ULONG(addr)));

	spin_lock(&shmem_lock);
	shmem = shmem_head;
	while (shmem) {
		if (shmem->pfn == phy2pfn(vtop(addr))) {
			free_shmem_in_block(shmem);
			if (tmp == NULL)
				shmem_head = shmem->next;
			else
				tmp->next = shmem->next;
			break;
			
		}
		tmp = shmem;
		shmem = shmem->next;
	}

	if (shmem == NULL)
		pr_err("not a shemm 0x%x\n", ULONG(addr));

	spin_unlock(&shmem_lock);
}
