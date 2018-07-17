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

#include <minos/io.h>
#include <drivers/pl011.h>
#include <minos/mmu.h>
#include <minos/init.h>

static void *base = (void *)0x1c090000;
extern void flush_log_buf(void);

int uart_init(void)
{
	iowrite32(base + UARTCR, 0x0);
	iowrite32(base + UARTECR, 0x0);
	iowrite32(base + UARTLCR_H, 0x0 | PL011_LCR_WORD_LENGTH_8 | \
			PL011_LCR_ONE_STOP_BIT | \
			PL011_LCR_PARITY_DISABLE | \
			PL011_LCR_BREAK_DISABLE);
	iowrite32(base + UARTIBRD, PL011_IBRD_DIV_38400);
	iowrite32(base + UARTFBRD, PL011_FBRD_DIV_38400);

	iowrite32(base + UARTIMSC, 0x0);
	iowrite32(base + UARTICR, PL011_ICR_CLR_ALL_IRQS);
	iowrite32(base + UARTCR, 0x0 | PL011_CR_UART_ENABLE | \
			PL011_CR_TX_ENABLE | \
			PL011_CR_RX_ENABLE);

	return 0;
}

void uart_putc(char c)
{
	while (ioread32(base + UARTFR) & PL011_FR_BUSY_FLAG);

	if (c == '\n')
		iowrite32(base + UARTDR, '\r');

	iowrite32(base + UARTDR, c);
}

char uart_getchar(void)
{
	while (ioread32(base + UARTFR) & PL011_FR_BUSY_FLAG);

	return ioread32(base + UARTDR);
}
