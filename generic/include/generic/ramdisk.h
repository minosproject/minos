#ifndef __LIBMINOS_RAMDISK_H__
#define __LIBMINOS_RAMDISK_H__

#ifdef __KERNEL__
#include <minos/types.h>
#else
#include <inttypes.h>
#include <sys/types.h>
#endif

#define RAMDISK_MAGIC "MINOSRAMDISK...."
#define RAMDISK_MAGIC_SIZE 16

#define RAMDISK_FNAME_SIZE 32

struct ramdisk_inode {
	char f_name[RAMDISK_FNAME_SIZE];
	uint64_t f_offset;	// data offset from ramdisk_start.
	uint64_t f_size;	// data size of this file
} __attribute__((__packed__));

struct ramdisk_sb {
	uint32_t file_cnt;
	uint32_t block_size;	// always 4096
	uint64_t inode_offset;	// inode offset
	uint64_t data_offset;	// file data offset.
	uint64_t ramdisk_size;	// total size of the ramdisk.
};

struct ramdisk_file {
	struct ramdisk_inode *inode;
	unsigned long pos;	// reserved
};

#endif
