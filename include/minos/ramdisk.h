#ifndef __MINOS_RAMDISK_H__
#define __MINOS_RAMDISK_H__

#include <generic/ramdisk.h>

extern void *ramdisk_start, *ramdisk_end;

void set_ramdisk_address(void *start, void *end);

int ramdisk_init(void);

int ramdisk_read(struct ramdisk_file *file, void *buf,
		size_t size, unsigned long offset);

int ramdisk_open(char *name, struct ramdisk_file *file);

unsigned long ramdisk_file_base(struct ramdisk_file *file);

unsigned long ramdisk_file_size(struct ramdisk_file *file);

const char *ramdisk_file_name(struct ramdisk_file *file);

#endif
