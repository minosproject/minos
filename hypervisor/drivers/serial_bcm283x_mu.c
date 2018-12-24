/*
 * copyright (c) 2018 min le (lemin9538@gmail.com)
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license version 2 as
 * published by the free software foundation.
 *
 * this program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose.  see the
 * gnu general public license for more details.
 *
 * you should have received a copy of the gnu general public license
 * along with this program.  if not, see <http://www.gnu.org/licenses/>.
 */

#include <minos/errno.h>
#include <asm/io.h>
#include <minos/mmu.h>
#include <minos/init.h>

#define AUX_MU_IO	0x00
#define AUX_MU_IER	0x04
#define AUX_MU_IIR	0x08
#define AUX_MU_LCR	0x0c
#define AUX_MU_MCR	0x10
#define AUX_MU_LSR	0x14
#define AUX_MU_MSR	0x18
#define AUX_MU_SCRATCH	0x1c
#define AUX_MU_CNTL	0x20
#define AUX_MU_STAT	0x24
#define AUX_MU_BAUD	0x28

#define BCM283X_LCR_DATA_SIZE_8		3
#define BCM283X_MU_LSR_TX_EMPTY		(1 << 5)
#define BCM283X_MU_LSR_RX_READY		(1 << 0)

static void *serial_base;

static inline void __bcm283x_mu_putc(char c)
{
	while (!(ioread32(serial_base + AUX_MU_LSR) &
				BCM283X_MU_LSR_TX_EMPTY));

	iowrite32(c, serial_base + AUX_MU_IO);
}

int bcm283x_mu_putc(char c)
{
	if (c == '\n')
		__bcm283x_mu_putc('\r');

	__bcm283x_mu_putc(c);
	return 0;
}

char bcm283x_mu_getc(void)
{
	uint32_t data;

	if (!(ioread32(serial_base + AUX_MU_LSR) &
				BCM283X_MU_LSR_RX_READY))
		return -EAGAIN;

	data = ioread32(serial_base + AUX_MU_IO);
	return (int)data;
}

int bcm283x_mu_init(void *base, int clk, int baud)
{
	uint32_t divider;

	if (!base)
		return -EINVAL;

	serial_base = base;
	divider = clk / (baud * 8);

	iowrite32(BCM283X_LCR_DATA_SIZE_8, serial_base + AUX_MU_LCR);
	iowrite32(divider - 1, serial_base + AUX_MU_BAUD);

	return 0;
}
