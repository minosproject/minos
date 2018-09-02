#ifndef __MINOS_SERIAL_H__
#define __MINOS_SERIAL_H__

void serial_putc(char c);
char serial_getc(void);
int serial_init(void);

#endif
