#ifndef _MINOS_MMU_H_
#define _MINOS_MMU_H_

#include <minos/types.h>
#include <minos/memattr.h>
#include <asm/pagetable.h>

#define PGD_RANGE_OFFSET	(__PGD_RANGE_OFFSET)
#define PGD_DES_OFFSET		(__PGD_DES_OFFSET)
#define PGD_ENTRY_OFFSET_MASK	(__PGD_ENTRY_OFFSET_MASK)

#define PUD_RANGE_OFFSET	(__PUD_RANGE_OFFSET)
#define PUD_DES_OFFSET		(__PUD_DES_OFFSET)
#define PUD_ENTRY_OFFSET_MASK	(__PUD_ENTRY_OFFSET_MASK)

#define PMD_RANGE_OFFSET	(__PMD_RANGE_OFFSET)
#define PMD_DES_OFFSET		(__PMD_DES_OFFSET)
#define PMD_ENTRY_OFFSET_MASK	(__PMD_ENTRY_OFFSET_MASK)

#define PTE_RANGE_OFFSET	(__PTE_RANGE_OFFSET)
#define PTE_DES_OFFSET		(__PTE_DES_OFFSET)
#define PTE_ENTRY_OFFSET_MASK	(__PTE_ENTRY_OFFSET_MASK)

#define PAGETABLE_ATTR_MASK 	(__PAGETABLE_ATTR_MASK)

#define PGD_MAP_SIZE		(1UL << PGD_RANGE_OFFSET)
#define PUD_MAP_SIZE		(1UL << PUD_RANGE_OFFSET)
#define PMD_MAP_SIZE		(1UL << PMD_RANGE_OFFSET)
#define PTE_MAP_SIZE		(1UL << PTE_RANGE_OFFSET)

#define PGD_NOT_MAPPED		(PGD + 1)
#define PUD_NOT_MAPPED		(PUD + 1)
#define PMD_NOT_MAPPED		(PMD + 1)
#define PTE_NOT_MAPPED		(PTE + 1)
#define INVALID_MAPPING		(6)

#define mapping_error(r)	(((unsigned long)(r) > 0) && ((unsigned long)(r) <= 6))

struct vm;

struct mapping_struct {
	unsigned long table_base;
	unsigned long vir_base;
	unsigned long phy_base;
	size_t size;
	int lvl;
	int host;
	int mem_type;
	void *data;
	void *(*get_free_pages)(int pages, void *data);
	struct pagetable_attr *config;
};

int create_mem_mapping(struct mapping_struct *);
int destroy_mem_mapping(struct mapping_struct *);
unsigned long get_mapping_entry(unsigned long tt,
		unsigned long vir, int start, int end);

void create_level_mapping(int lvl, unsigned long tt, unsigned long addr,
		int mem_type, int map_type, int host);

unsigned long get_tt_description(int host, int m_type, int d_type);

static inline unsigned long
get_mapping_pte(unsigned long tt, unsigned long vir, int host)
{
	if (host)
		return INVALID_MAPPING;

	return get_mapping_entry(tt, vir, PUD, PTE);
}

static inline unsigned long
get_mapping_pgd(unsigned long tt, unsigned long vir, int host)
{
	if (!host)
		return INVALID_MAPPING;

	return get_mapping_entry(tt, vir, PGD, PGD);
}

static inline unsigned long
get_mapping_pud(unsigned long tt, unsigned long vir, int host)
{
	int start, end;

	if (host) {
		start = PGD;
		end = PUD;
	} else {
		start = PUD;
		end = PUD;
	}

	return get_mapping_entry(tt, vir, start, end);
}

static inline unsigned long
get_mapping_pmd(unsigned long tt, unsigned long vir, int host)
{
	int start, end;

	if (host) {
		start = PGD;
		end = PMD;
	} else {
		start = PUD;
		end = PMD;
	}

	return get_mapping_entry(tt, vir, start, end);
}

#endif
