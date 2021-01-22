// SPDX-License-Identifier: GPL-2.0

#include <asm/io.h>
#include <minos/console.h>

/* Register offsets */
#define SCIF_SCFTDR    (0x0C)      /* Transmit FIFO data register */
#define SCIF_SCFSR     (0x10)      /* Serial status register */
#define SCIF_SCFRDR    (0x14)      /* Receive FIFO data register */

/* Serial Status Register (SCFSR) */
#define SCFSR_TEND     (1 << 6)    /* Transmission End */
#define SCFSR_RDF      (1 << 1)    /* Receive FIFO Data Full */
#define SCFSR_DR       (1 << 0)    /* Receive Data Ready */

static void *serial_base = (void *)CONFIG_UART_BASE;

static inline void __scif_serial_putc(char c)
{
    /* Check for empty space in TX FIFO */
    while (!(readw(serial_base + SCIF_SCFSR) & SCFSR_TEND))
        ;

    writeb(c, serial_base + SCIF_SCFTDR);
    /* Clear required TX flags */
    writew(readw(serial_base + SCIF_SCFSR) & ~SCFSR_TEND,
           serial_base + SCIF_SCFSR);
}

static void scif_serial_putc(char c)
{
    if (c == '\n')
        __scif_serial_putc('\r');

    __scif_serial_putc(c);
}

static char scif_serial_getc(void)
{
    /* Check for available data bytes in RX FIFO */
    if (!(readw(serial_base + SCIF_SCFSR) & (SCFSR_DR | SCFSR_RDF)))
        return 0;

    char c = readb(serial_base + SCIF_SCFRDR);
    /* dummy read */
    readw(serial_base + SCIF_SCFSR);

    /* Clear required RX flags */
    writew(~(SCFSR_DR | SCFSR_RDF), serial_base + SCIF_SCFSR);

    return c;
}

DEFINE_CONSOLE(scif, "renesas,scif", NULL, scif_serial_putc, scif_serial_getc);
