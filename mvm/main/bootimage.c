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

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <bootimage.h>
#include <mvm.h>

static void dump_bootimg_hdr(boot_img_hdr *hdr)
{
	char buf[1024];

	memset(buf, 0, 1024);
	strncpy(buf, (char *)hdr->magic, BOOT_MAGIC_SIZE);

	pr_info("magic        - %s\n", buf);
	pr_info("kernel_size  - 0x%x\n", hdr->kernel_size);
	pr_info("kernel_addr  - 0x%x\n", hdr->kernel_addr);
	pr_info("ramdisk_size - 0x%x\n", hdr->ramdisk_size);
	pr_info("ramdisk_addr - 0x%x\n", hdr->ramdisk_addr);
	pr_info("dtb_size     - 0x%x\n", hdr->second_size);
	pr_info("dtb_addr     - 0x%x\n", hdr->second_addr);
	pr_info("tags_addr    - 0x%x\n", hdr->tags_addr);
	pr_info("page_size    - 0x%x\n", hdr->page_size);

	strncpy(buf, (char *)hdr->name, BOOT_NAME_SIZE);
	pr_info("name         - %s\n", buf);
	strncpy(buf, (char *)hdr->cmdline, BOOT_ARGS_SIZE);
	pr_info("cmdline      - %s\n", buf);
}

int read_bootimage_header(int fd, boot_img_hdr *hdr)
{
	int ret;
	struct stat stbuf;
	unsigned long page_size;
	unsigned long k, r, o, t;

	ret = read(fd, hdr, sizeof(boot_img_hdr));
	if (ret != sizeof(boot_img_hdr))
		return -EIO;

	/* here check the header */
	if (strncmp((char *)hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE) != 0)
		return 1;

	if (!hdr->kernel_size)
		return 1;

	if (!hdr->page_size)
		return 1;

	page_size = hdr->page_size;
	k = (hdr->kernel_size + page_size - 1) / page_size;
	r = (hdr->ramdisk_size + page_size - 1) / page_size;
	o = (hdr->second_size + page_size - 1) / page_size;
	t = (1 + k + r + o) * page_size;

	if ((fstat(fd, &stbuf) != 0) || (!S_ISREG(stbuf.st_mode)))
		return 1;

	if (t > stbuf.st_size)
		return -EINVAL;

	dump_bootimg_hdr(hdr);

	return 0;
}
