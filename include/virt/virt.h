#ifndef __MINOS_VIRT_H__
#define __MINOS_VIRT_H__

#include <asm/virt.h>

struct task;

int virt_init(void);

void start_all_vm(void);

#endif
