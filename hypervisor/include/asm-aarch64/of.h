#ifndef __MINOS_OF_H__
#define __MINOS_OF_H__

int of_get_u64_array(char *path, char *attr, uint64_t *array, int *len);
int of_get_u32_array(char *path, char *attr, uint32_t *array, int *len);
int of_get_interrupt_regs(int node, uint64_t *array, int *array_len);
const char *of_get_compatible(int node);
int of_get_node_by_name(int pnode, char *str, int deepth);

#endif
