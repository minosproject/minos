#ifndef _MINOS_ARM_FVP_UART_H_
#define _MINOS_ARM_FVP_UART_H_

#define UARTDR		0x0
#define UARTECR		0x4
#define UARTFR		0x18
#define UARTILPR	0x20
#define UARTIBRD	0x24
#define UARTFBRD	0x28
#define UARTLCR_H	0x2c
#define UARTCR		0x30
#define UARTIFLS	0x34
#define UARTIMSC	0x38
#define UARTRIS		0x3c
#define UARTMIS		0x40
#define UARTICR		0x44
#define UARTDMACR	0x48

/*
 * defines for control/status registers
 */
#define PL011_LCR_WORD_LENGTH_8   (0x60)
#define PL011_LCR_WORD_LENGTH_7   (0x40)
#define PL011_LCR_WORD_LENGTH_6   (0x20)
#define PL011_LCR_WORD_LENGTH_5   (0x00)

#define PL011_LCR_FIFO_ENABLE     (0x10)
#define PL011_LCR_FIFO_DISABLE    (0x00)

#define PL011_LCR_TWO_STOP_BITS   (0x08)
#define PL011_LCR_ONE_STOP_BIT    (0x00)

#define PL011_LCR_PARITY_ENABLE   (0x02)
#define PL011_LCR_PARITY_DISABLE  (0x00)

#define PL011_LCR_BREAK_ENABLE    (0x01)
#define PL011_LCR_BREAK_DISABLE   (0x00)

#define PL011_IBRD_DIV_38400      (0x27)
#define PL011_FBRD_DIV_38400      (0x09)

#define PL011_ICR_CLR_ALL_IRQS    (0x07FF)

#define PL011_FR_BUSY_FLAG        (0x08)
#define PL011_FR_RXFE_FLAG        (0x10)
#define PL011_FR_TXFF_FLAG        (0x20)
#define PL011_FR_RXFF_FLAG        (0x40)
#define PL011_FR_TXFE_FLAG        (0x80)

#define PL011_CR_UART_ENABLE      (0x01)

#define PL011_CR_TX_ENABLE        (0x0100)
#define PL011_CR_RX_ENABLE        (0x0200)

void uart_putc(char c);
char uart_getchar(void);

#endif
