/*
 * Copyright (C) 2022 Min Le (lemin9538@gmail.com)
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include <generic/ramdisk.h>

#define MODE_NULL 0x0
#define MODE_DIR 0x1
#define MODE_FILE 0x2

struct ramdisk_sb ramdisk_sb;

struct memblock {
	size_t size;
	size_t free;
	size_t used;
	void *mem;
};

struct memblock imem;
struct memblock fmem;

#define FMEMBLOCK_SIZE 128 * 1024 * 1024
#define IMEMBLOCK_SIZE 32 * 1024 * 1024

static void print_help(void)
{
	printf("mkrmd Usage:\n");
	printf("    mkrmd -d $target /tmp/ramdisk/\n");
	printf("    mkrmd -f $target vm0.img vm1.img\n");
}

static void memblock_init(void)
{
	imem.used = 0;
	imem.size = imem.free = IMEMBLOCK_SIZE;
	imem.mem = malloc(imem.free);

	fmem.used = 0;
	fmem.size = fmem.free = FMEMBLOCK_SIZE;
	fmem.mem = malloc(fmem.free);

	if (!imem.mem || !fmem.mem) {
		printf("alloc memory for ramdisk failed\n");
		exit(-ENOMEM);
	}
}

static int add_new_file(char *name, size_t size)
{
	struct ramdisk_inode *inode;
	size_t total_size;

	if (imem.free <= sizeof(struct ramdisk_inode))
		return -ENOMEM;

	inode = (struct ramdisk_inode *)(imem.mem + imem.used);
	inode->f_offset = fmem.used;
	inode->f_size = size;
	memset(inode->f_name, 0, RAMDISK_FNAME_SIZE);
	strcpy(inode->f_name, name);

	printf("%s f_offset 0x%ld 0x%ld\n", name,
			inode->f_offset, inode->f_size);

	imem.used += sizeof(struct ramdisk_inode);
	imem.free -= sizeof(struct ramdisk_inode);

	/*
	 * whether read() will read last char EOF of an file ?
	 * if not, need add it.
	 */
	ramdisk_sb.file_cnt += 1;

	/*
	 * file memory need PAGE_SIZE (4096) aligned.
	 */
	total_size = (size + 4095) & ~(4095);
	fmem.used += size;

	/*
	 * adjust it.
	 */
	size = total_size - size;
	if (size > 0) {
		memset(fmem.mem + fmem.used, 0, size);
		fmem.used += size;
		fmem.free -= size;
	}

	return 0;
}

static int __add_file(int fd, char *name)
{
	size_t size = 0, cnt, read_size;
	char *base = fmem.mem + fmem.used;

	while (1) {
		if (fmem.free == 0) {
			printf("no more memory in fmemblock\n");
			return -ENOMEM;
		}

		read_size = fmem.free > 4096 ? 4096 : fmem.free;
		cnt = read(fd, base, read_size);
		if (cnt == 0)
			break;

		size += cnt;
		base += cnt;
		fmem.free -= cnt;
	}

	if (size == 0) {
		printf("%s size is 0\n", name);
		return 0;
	}

	if (add_new_file(name, size)) {
		printf("no more memory in inode memory\n");
		return -ENOMEM;
	}

	return 0;
}

static int add_file(char *path)
{
	char *name;
	int fd, ret;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		printf("open %s failed\n", path);
		return fd;
	}

	/*
	 * get the ramdisk file name.
	 */
	name = strrchr(path, '/');
	if (name == NULL)
		name = path;
	else
		name = name + 1;

	ret = __add_file(fd, name);

	close(fd);

	return ret;
}

static int make_ramdisk_file_mode(int cnt, char **filep)
{
	int i;

	for (i = 0; i < cnt; i++) {
		if (add_file(filep[i]))
			return -ENOENT;
	}

	return 0;
}

static int make_ramdisk_dir_mode(char *dirname)
{
	struct dirent *dirp;
	char path[256];
	DIR *dp;

	if (dirname[strlen(dirname) - 1] == '/')
		dirname[strlen(dirname)] = 0;

	dp = opendir(dirname);
	if (dp == NULL) {
		printf("can not open directory %s\n", dirname);
		return -ENOENT;
	}

	while ((dirp = readdir(dp)) != NULL) {
		if (!strcmp(dirp->d_name, ".") || !strcmp(dirp->d_name, ".."))
			continue;
		
		if (dirp->d_type != 8) {
			printf("skip %s\n", dirp->d_name);
			continue;
		}

		if ((strlen(dirname) + strlen(dirp->d_name)) > 255 ||
				(strlen(dirp->d_name) + 1) > RAMDISK_FNAME_SIZE) {
			printf("skip file %s: filename too long\n", dirp->d_name);
			continue;
		}

		sprintf(path, "%s/%s", dirname, dirp->d_name);
		add_file(path);
	}

	closedir(dp);

	return 0;
}

static int packing_ramdisk(int fd)
{
	/*
	 * write the ramdisk magic.
	 */
	char magic[RAMDISK_MAGIC_SIZE] = {0};
	unsigned long inode_offset;
	unsigned long file_offset;
	int ret;

	memcpy(magic, RAMDISK_MAGIC, RAMDISK_MAGIC_SIZE);
	ret = write(fd, magic, RAMDISK_MAGIC_SIZE);
	if (ret != RAMDISK_MAGIC_SIZE) {
		printf("write ramdisk magic failed\n");
		return -EIO;
	}

	inode_offset = RAMDISK_MAGIC_SIZE + sizeof(struct ramdisk_sb);
	inode_offset = (inode_offset + 15) & ~(15);	// 16 byte align
	ramdisk_sb.inode_offset = inode_offset;

	file_offset = inode_offset + imem.used;
	file_offset = (file_offset + 4095) & ~(4095);	// 4096 byte align
	ramdisk_sb.data_offset = file_offset; 

	/*
	 * write the superblock.
	 */
	ret = write(fd, &ramdisk_sb, sizeof(struct ramdisk_sb));
	if (ret != sizeof(struct ramdisk_sb)) {
		printf("write ramdisk super block failed\n");
		return -EIO;
	}

	/*
	 * write inode.
	 */
	ret = lseek(fd, inode_offset, SEEK_SET);
	if (ret < 0) {
		printf("seek indoe offset failed\n");
		return -EIO;
	}

	ret = write(fd, imem.mem, imem.used);
	if (ret != imem.used) {
		printf("write inode data failed\n");
		return -EIO;
	}

	/*
	 * write data
	 */
	ret = lseek(fd, file_offset, SEEK_SET);
	if (ret < 0) {
		printf("seek file offset failed\n");
		return -EIO;
	}

	ret = write(fd, fmem.mem, fmem.used);
	if (ret != fmem.used) {
		printf("write file data failed\n");
		return -EIO;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int mode = MODE_NULL;
	int ret;
	int target;

	if (argc < 4) {
		print_help();
		exit(-1);
	}

	if (strcmp(argv[1], "-d") == 0)
		mode = MODE_DIR;
	else if (strcmp(argv[1], "-f") == 0)
		mode = MODE_FILE;

	if (mode == MODE_NULL) {
		printf("unsupported mode\n");
		print_help();
		exit(-1);
	}

	printf("open or create target file [%s]\n", argv[2]);
	target = open(argv[2], O_RDWR | O_CREAT, 0666);
	if (target < 0) {
		printf("can not open target file %d\n", errno);
		return target;
	}

	memblock_init();

	printf("create ramdisk using %s mode\n", mode == MODE_DIR ? "DIR" : "FILE");
	if (mode == MODE_DIR)
		ret = make_ramdisk_dir_mode(argv[3]);
	else
		ret = make_ramdisk_file_mode(argc - 3, &argv[3]);
	if (ret) {
		printf("create %s failed wit %d\n", argv[2], ret);
		goto out;
	}

	ret = packing_ramdisk(target);
	if (!ret) {
		printf("generate ramdisk %s done\n", argv[2]);
		printf("file_cnt %d inode_offset 0x%lx data_offset 0x%lx\n",
					ramdisk_sb.file_cnt,
					ramdisk_sb.inode_offset,
					ramdisk_sb.data_offset);
	}

out:
	close(target);

	return ret;
}
