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

#include <minos/minos.h>
#include <asm/io.h>
#include <minos/console.h>

struct meson_uart {
	u32 wfifo;
	u32 rfifo;
	u32 control;
	u32 status;
	u32 misc;
};

/* AML_UART_STATUS bits */
#define AML_UART_PARITY_ERR		BIT(16)
#define AML_UART_FRAME_ERR		BIT(17)
#define AML_UART_TX_FIFO_WERR		BIT(18)
#define AML_UART_RX_EMPTY		BIT(20)
#define AML_UART_TX_FULL		BIT(21)
#define AML_UART_TX_EMPTY		BIT(22)
#define AML_UART_XMIT_BUSY		BIT(25)
#define AML_UART_ERR			(AML_UART_PARITY_ERR | \
					 AML_UART_FRAME_ERR  | \
					 AML_UART_TX_FIFO_WERR)
/* AML_UART_CONTROL bits */
#define AML_UART_TX_EN			BIT(12)
#define AML_UART_RX_EN			BIT(13)
#define AML_UART_TX_RST			BIT(22)
#define AML_UART_RX_RST			BIT(23)
#define AML_UART_CLR_ERR		BIT(24)

static volatile struct meson_uart *uart = (struct meson_uart *)ptov(0xff803000);

static int meson_serial_init(char *arg)
{
#if 0
	u32 val;

	val = readl(&uart->control);
	val |= (AML_UART_RX_RST | AML_UART_TX_RST | AML_UART_CLR_ERR);
	writel(val, &uart->control);
	val &= ~(AML_UART_RX_RST | AML_UART_TX_RST | AML_UART_CLR_ERR);
	writel(val, &uart->control);
	val |= (AML_UART_RX_EN | AML_UART_TX_EN);
	writel(val, &uart->control);
#endif

	return 0;
}

static char meson_serial_getc(void)
{
	if ((readl(&uart->status) & 0x7f) == 0)
		return 0;

	return readl(&uart->rfifo) & 0xff;
}

static inline void __meson_serial_putc(char c)
{
	while (readl(&uart->status) & AML_UART_TX_FULL);

	writel(c, &uart->wfifo);
}

static void meson_serial_putc(char c)
{
	if (c == '\n')
		__meson_serial_putc('\r');

	__meson_serial_putc(c);
}
DEFINE_CONSOLE(aml_meson, "aml_meson", meson_serial_init,
		meson_serial_putc, meson_serial_getc);
