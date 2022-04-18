#ifndef __MINOS_VIRT_H__
#define __MINOS_VIRT_H__

#include <asm/virt.h>

struct mm_struct;

int virt_init(void);
void start_all_vm(void);

void flush_all_tlb_mm(struct mm_struct *mm);

#endif
