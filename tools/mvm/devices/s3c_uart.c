/*
 * BSD 3-Clause License
 *
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <mvm.h>
#include <sys/ioctl.h>
#include <vdev.h>
#include <list.h>
#include <sys/mman.h>
#include <libfdt/libfdt.h>
#include <common/gvm.h>
#include <errno.h>

/*
 * return 0 or < 0 means fail
 * return bigger than 0 means sucessful
 *
 * the argument string must follow below format:
 * ./mvm vcpu=2; device=s3c_uart,iomem=0x0,iosize=0x0;
 */
int get_opt_string(char *buf, char *arg, char *buf)
{

}

int get_opt_u64(char *buf, char *arg, uint64_t *value)
{
	return 0;
}

static int s3c_uart_init(struct vdev *vdev, char *args)
{
	uint64_t base, size = 4096;
	char *tmp = args;

	if (!args) {
		pr_warn("no vaild address info for uart\n");
		return -EINVAL;
	}
}

int create_s3c_uart(struct vm *vm, uint64_t base, uint64_t size)
{
	char buf[128];

	memset(buf, 128, sizeof(buf));
	sscanf("iobase=0x%x,iosize=0x%x", base, size);

	return create_vdev(vm, "s3c_uart", buf);
}

static struct vdev_ops s3c_uart_ops = {
	.name = "s3c_uart",
	.init = s3c_uart_init,
	.deinit = s3c_uart_deinit,
	.reset = s3c_uart_reset,
	.setup = s3c_uart_setup,
	.event = s3c_uart_event,
};
DEFINE_VDEV_TYPE(s3c_uart_ops);
