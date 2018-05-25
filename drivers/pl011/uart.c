#include <minos/io.h>
#include <drivers/pl011.h>
#include <minos/mmu.h>
#include <minos/init.h>

static void *base = (void *)0x1c090000;
extern void flush_log_buf(void);

static int uart_init(void)
{
	io_remap(0x1c090000, 0x1c090000, 64 * 1024);
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

	flush_log_buf();

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

early_initcall(uart_init);
