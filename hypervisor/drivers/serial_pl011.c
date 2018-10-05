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

#include <minos/errno.h>
#include <asm/io.h>
#include <drivers/pl011.h>
#include <minos/mmu.h>
#include <minos/init.h>

static void *base = NULL;

int pl011_init(void *addr)
{
	if (!addr)
		return -EINVAL;

	base = addr;

	iowrite32(0x0, base + UARTCR);
	iowrite32(0x0, base + UARTECR);
	iowrite32(0x0 | PL011_LCR_WORD_LENGTH_8 | \
		  PL011_LCR_ONE_STOP_BIT | \
		  PL011_LCR_PARITY_DISABLE | \
		  PL011_LCR_BREAK_DISABLE, base + UARTLCR_H);
	iowrite32(PL011_IBRD_DIV_38400, base + UARTIBRD);
	iowrite32(PL011_FBRD_DIV_38400, base + UARTFBRD);

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
