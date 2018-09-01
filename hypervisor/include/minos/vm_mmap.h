#ifndef __MINOS_VM_MMAP_H__
#define __MINOS_VM_MMAP_H__

#include <config/config.h>

/*
 * guest vm memory map
 *
 * |-------------------------| -- 0x00000000
 * | 2G IO MEM	             |
 * |-------------------------| -- 0x80000000
 * |		             |
 * | 126G DRAM/NORMAL MEM    |
 * |-------------------------| -- 0x2000000000
 * | 2G GUEST VM IO MMAP     |
 * |-------------------------| -- 0x2080000000
 * | GUEST VM MMAP           |
 * |-------------------------| -- 0x10000000000
 *
 */

#define GVM_PHYSIC_MEM_RANGE		(1UL << CONFIG_PLATFORM_ADDRESS_RANGE)
#define GVM_PHYSIC_MEM_START		(0x0000000000000000UL)

#define GVM_IO_MEM_START		(0x00000000UL)
#define GVM_IO_MEM_SIZE			(0x80000000UL)

#define GVM_NORMAL_MEM_START		(GVM_IO_MEM_START + GVM_IO_MEM_SIZE)
#define GVM_NORMAL_MEM_SIZE		(126 * 1024 * 1024 * 1024UL)
#define GVM_NORMAL_MEM_END		(GVM_NORMAL_MEM_START + GVM_NORMAL_MEM_SIZE)

#define HVM_IO_MMAP_START		(GVM_NORMAL_MEM_END)
#define HVM_IO_MMAP_SIZE		(2 * 1024 * 1024 * 1024UL)

#define HVM_NORMAL_MMAP_START		(HVM_IO_MMAP_START + HVM_IO_MMAP_SIZE)
#define HVM_NORMAL_MMAP_SIZE		(GVM_PHYSIC_MEM_RANGE - HVM_NORMAL_MMAP_START)

#define GVM_IOMEM_BASE(addr)		(addr - CONFIG_PLATFORM_IO_BASE + GVM_IO_MEM_START)
#define GVM_MEM_BASE(addr)		(addr - CONFIG_PLATFORM_DRAM_BASE + GVM_NORMAL_MEM_START)

#endif
