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

#define T7001_S3C_UART_BASE	0x20a0c0000
#define T7001_S3C_UART_SIZE	0x4000

#define ULCON     0x00      //UART 0 Line control
#define UCON      0x04      //UART 0 Control
#define UFCON     0x08      //UART 0 FIFO control
#define UMCON     0x0c      //UART 0 Modem control
#define UTRSTAT   0x10      //UART 0 Tx/Rx status
#define UERSTAT   0x14      //UART 0 Rx error status
#define UFSTAT    0x18      //UART 0 FIFO status
#define UMSTAT    0x1c      //UART 0 Modem status
#define UTXH      0x20	    //UART 0 Transmission Hold
#define URXH      0x24      //UART 0 Receive buffer
#define UBRDIV    0x28      //UART 0 Baud rate divisor
#define UDIVSLOT  0x2C      //UART 0 Baud rate divisor

/*
 * return 0 or < 0 means fail
 * return bigger than 0 means sucessful
 *
 * the argument string must follow below format:
 * ./mvm vcpu=2; device=s3c_uart,iomem=0x0,iosize=0x0;
 */
static int s3c_uart_init(struct vdev *vdev, char *args)
{
	pr_info("create s3c uart\n");

	vdev->guest_iomem = (void *)T7001_S3C_UART_BASE;
	vdev->iomem_size = T7001_S3C_UART_SIZE;

	return 0;
}

static int s3c_uart_setup(struct vdev *vdev, void *data, int os)
{
	return 0;
}

static void s3c_uart_deinit(struct vdev *vdev)
{

}

static int s3c_uart_reset(struct vdev *vdev)
{
	return 0;
}

int create_s3c_uart(struct vm *vm)
{
	return create_vdev(vm, "s3c_uart", NULL);
}

static void s3c_uart_read_event(unsigned long reg, unsigned long *value)
{
	switch (reg) {
	case UTRSTAT:
		*value = 5;
		break;
	case URXH:
		*value = 0;
		break;
	default:
		break;
	}
}

static void s3c_uart_write_event(unsigned long reg, unsigned long *value)
{
	switch (reg) {
	case UTXH:
		printf("%c", (unsigned char)(*value));
		break;
	default:
		break;
	}
}

static int s3c_uart_event(struct vdev *vdev, int read,
		unsigned long addr, unsigned long *value)
{
	unsigned long reg = addr - (unsigned long)vdev->guest_iomem;

	if (read == 0)
		s3c_uart_read_event(reg, value);
	else
		s3c_uart_write_event(reg, value);

	return 0;
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
