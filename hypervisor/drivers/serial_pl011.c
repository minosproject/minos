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
#include <drivers/pl011.h>
#include <minos/mmu.h>
#include <minos/init.h>
#include <config/config.h>

static void *base = (void *)CONFIG_UART_BASE;

int pl011_init(void *addr, int clock, int baudrate)
{
	unsigned int temp;
	unsigned int divider;
	unsigned int remainder;
	unsigned int fraction;

	if (!addr)
		return -EINVAL;

	base = addr;
	temp = 16 * baudrate;
	divider = clock / temp;
	remainder = clock % temp;
	temp = (8 * remainder) / baudrate;
	fraction = (temp >> 1) + (temp & 1);

	iowrite32(0x0, base + UARTCR);
	iowrite32(0x0, base + UARTECR);
	iowrite32(0x0 | PL011_LCR_WORD_LENGTH_8 | \
		  PL011_LCR_ONE_STOP_BIT | \
		  PL011_LCR_PARITY_DISABLE | \
		  PL011_LCR_BREAK_DISABLE, base + UARTLCR_H);

	iowrite32(divider, base + UARTIBRD);
	iowrite32(fraction, base + UARTFBRD);

	iowrite32(0X0, base + UARTIMSC);
	iowrite32(PL011_ICR_CLR_ALL_IRQS, base + UARTICR);
	iowrite32(0x0 | PL011_CR_UART_ENABLE | \
		  PL011_CR_TX_ENABLE | \
		  PL011_CR_RX_ENABLE, base + UARTCR);

	return 0;
}

void serial_pl011_putc(char c)
{
	while (ioread32(base + UARTFR) & PL011_FR_BUSY_FLAG);

	if (c == '\n')
		iowrite32('\r', base + UARTDR);

	iowrite32(c, base + UARTDR);
}

char serial_pl011_getc(void)
{
	while (ioread32(base + UARTFR) & PL011_FR_BUSY_FLAG);

	return ioread32(base + UARTDR);
}
