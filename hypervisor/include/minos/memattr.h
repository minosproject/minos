#ifndef __MINOS_MEM_ATTR_H__
#define __MINOS_MEM_ATTR_H__

#define GUEST_PGD_PAGE_NR	(__GUEST_PGD_PAGE_NR)
#define GUEST_PGD_PAGE_ALIGN	(__GUEST_PGD_PAGE_ALIGN)

#define DESCRIPTION_FAULT	(0x0)
#define DESCRIPTION_BLOCK	(0x1)
#define DESCRIPTION_PAGE	(0x2)
#define DESCRIPTION_TABLE	(0x3)

#define MEM_TYPE_NORMAL		(0x0)
#define MEM_TYPE_IO		(0x1)

#define MEM_REGION_NAME_SIZE	32

#define PGD			(0)
#define PUD			(1)
#define PMD			(2)
#define PTE			(3)

#define PAGE_NR(size)		(size >> PAGE_SHIFT)
#define __PAGE_MASK		(~((1UL << PAGE_SHIFT) - 1))

#define GFB_SLAB		(1 << 0)
#define GFB_PAGE		(1 << 1)
#define GPF_PAGE_META		(1 << 2)
#define GFB_VM			(1 << 3)

#define GFB_SLAB_BIT		(0)
#define GFB_PAGE_BIT		(1)
#define GFB_PAGE_META_BIT	(2)
#define GFB_VM_BIT		(3)

#define GFB_MASK		(0xffff)

#define MAX_MEM_SECTIONS	(10)
#define MEM_BLOCK_SIZE		(0x200000)
#define MEM_BLOCK_SHIFT		(21)
#define PAGES_IN_BLOCK		(MEM_BLOCK_SIZE >> PAGE_SHIFT)

#endif
