/*
 * BSD 3-Clause License
 *
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

	pr_info("boot image infomation :\n");
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
