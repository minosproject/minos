#ifndef __MINOS_RAMDISK_H__
#define __MINOS_RAMDISK_H__

#include <minos/types.h>

#define RAMDISK_MAGIC	"MINOSRAMDISK...."

struct ramdisk_inode {
	char fname[256];
	uint64_t f_offset;	// data offset from ramdisk_start.
	uint64_t f_size;	// data size of this file
}__packed__;

struct ramdisk_sb {
	uint32_t file_cnt;
	uint32_t block_size;
	uint64_t inode_offset;
	uint64_t data_base;
};

struct ramdisk_file {
	struct ramdisk_inode *inode;
	unsigned long pos;
};

extern void *ramdisk_start, *ramdisk_end;

void set_ramdisk_address(void *start, void *end);

int ramdisk_read(struct ramdisk_file *file, void *buf,
		size_t size, unsigned long offset);

int ramdisk_open(char *name, struct ramdisk_file *file);

#endif
