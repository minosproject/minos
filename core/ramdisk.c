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
#include <minos/ramdisk.h>
#include <minos/mm.h>

void *ramdisk_start, *ramdisk_end;
static struct ramdisk_inode *root;
static struct ramdisk_sb *sb;

void set_ramdisk_address(void *start, void *end)
{
	ramdisk_start = start;
	ramdisk_end = end;
}

int ramdisk_init(void)
{
	/*
	 * need remap the ramdisk memory space, if it
	 * is not in the kernel memory space TBD.
	 */
	if (!ramdisk_start || !ramdisk_end) {
		pr_err("ramdisk address is not set\n");
		return -EINVAL;
	}

	if (create_host_mapping(ptov(ramdisk_start), (unsigned long)ramdisk_start,
				ramdisk_end - ramdisk_start, VM_RO)) {
		pr_err("unable map ramdisk memory\n");
		return -ENOMEM;
	}

	ramdisk_start = (void *)ptov(ramdisk_start);
	ramdisk_end = (void *)ptov(ramdisk_end);

	if (strncmp(ramdisk_start, RAMDISK_MAGIC, RAMDISK_MAGIC_SIZE) != 0) {
		pr_err("bad ramdisk format\n");
		return -EBADF;
	}

	/*
	 * the ramdisk is read only, init the ramdisk
	 * information, inclue the superblock and the
	 * root inode
	 */
	sb = ramdisk_start + RAMDISK_MAGIC_SIZE;
	root = ramdisk_start + sb->inode_offset;

	return 0;
}

const char *ramdisk_file_name(struct ramdisk_file *file)
{
	return file->inode->f_name;
}

unsigned long ramdisk_file_size(struct ramdisk_file *file)
{
	return file->inode->f_size;
}

unsigned long ramdisk_file_base(struct ramdisk_file *file)
{
	return (unsigned long)ramdisk_start + file->inode->f_offset;
}

static struct ramdisk_inode *get_file_inode(const char *name)
{
	struct ramdisk_inode *inode;

	for (inode = root; inode < root + sb->file_cnt; inode++) {
		if (strncmp(inode->f_name, name, RAMDISK_FNAME_SIZE - 1) == 0)
			return inode;
	}

	return NULL;
}

int ramdisk_read(struct ramdisk_file *file, void *buf,
		size_t size, unsigned long offset)
{
	if (!file)
		return -EINVAL;

	if ((offset + size) > file->inode->f_size)
		return -EINVAL;

	memcpy(buf, ramdisk_start + file->inode->f_offset + offset, size);
	return 0;
}

int ramdisk_open(char *name, struct ramdisk_file *file)
{
	struct ramdisk_inode *inode;

	if (!sb) {
		pr_debug("super block not found\n");
		return -ENOENT;
	}

	inode = get_file_inode(name);
	if (!inode)
		return -ENOENT;

	memset(file, 0, sizeof(struct ramdisk_file));
	file->inode = inode;

	return 0;
}
