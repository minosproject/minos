#ifndef __MINOS_OF_H__
#define __MINOS_OF_H__

int of_get_u64_array(char *path, char *attr, uint64_t *array, int *len);
int of_get_u32_array(char *path, char *attr, uint32_t *array, int *len);
int of_get_interrupt_regs(uint64_t *array, int *array_len);

#endif
