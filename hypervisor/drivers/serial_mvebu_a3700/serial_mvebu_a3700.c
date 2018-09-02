/*
 * Copyright (C) 2016 Stefan Roese <sr@denx.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <minos/minos.h>
#include <minos/io.h>

static void *base = (void *)0xd0012000;

/*
 * Register offset
 */
/* REG_UART_A3700 */
#define UART_RX_REG			0x00
#define UART_TX_REG			0x04
#define UART_CTRL_REG			0x08
#define UART_STATUS_REG			0x0c
#define UART_BAUD_REG			0x10
#define UART_POSSR_REG			0x14

#define UART_STATUS_RX_RDY		0x10
#define UART_STATUS_TXFIFO_FULL		0x800

#define UART_CTRL_RXFIFO_RESET		0x4000
#define UART_CTRL_TXFIFO_RESET		0x8000

#define CONFIG_UART_BASE_CLOCK		25804800

/* REG_UART_A3700_EXT */
#define UART_EXT_RX_REG			0x18
#define UART_EXT_TX_REG			0x1c
#define UART_EXT_CTRL_REG		0x04
#define UART_EXT_STATUS_RX_RDY		0x4000

static inline void __serial_putc(char ch)
{
	while (ioread32(base + UART_STATUS_REG) & UART_STATUS_TXFIFO_FULL);

	iowrite32(base + UART_TX_REG, ch);
}

void serial_putc(char ch)
{
	if (ch == '\n')
		__serial_putc('\r');

	__serial_putc(ch);
}

char serial_getc(void)
{
	while (!(ioread32(base + UART_STATUS_REG) & UART_STATUS_RX_RDY));

	return ioread32(base + UART_RX_REG) & 0xff;
}

static int mvebu_serial_setbrg(int baudrate)
{
	/*
	 * Calculate divider
	 * baudrate = clock / 16 / divider
	 */
	iowrite32(base + UART_BAUD_REG, CONFIG_UART_BASE_CLOCK / baudrate / 16);

	/*
	 * Set Programmable Oversampling Stack to 0,
	 * UART defaults to 16x scheme
	 */
	iowrite32(base + UART_POSSR_REG, 0);

	return 0;
}

int mvebu_serial_probe(void *addr)
{
	base = addr;

	/* reset FIFOs */
	iowrite32(base + UART_CTRL_REG, UART_CTRL_RXFIFO_RESET |
			UART_CTRL_TXFIFO_RESET);

	/* No Parity, 1 Stop */
	iowrite32(base + UART_CTRL_REG, 0);

	/* set brg to 115200 */
	mvebu_serial_setbrg(115200);

	return 0;
}
