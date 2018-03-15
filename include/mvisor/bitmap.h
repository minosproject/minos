#ifndef _MVISOR_BITMAP_H_
#define _MVISOR_BITMAP_H_

#include <mvisor/types.h>

typedef enum bit_ops {
	BITS_CLEAR,
	BITS_SET,
	BITS_READ
} bit_ops_t;

#define bits_of(type)	(sizeof(type) * 8)
#define bits_to_long(n)  \
	(((((n) + 8) & 0xffffffc0) / (sizeof(uint32_t) * 8)))
#define DECLARE_BITMAP(name, n) \
	uint32_t name[bits_to_long(n)]

int op_bits(uint32_t *bit_map, int n, bit_ops_t ops);

static void inline clear_bit(uint32_t *bit_map, int n)
{
	op_bits(bit_map, n, BITS_CLEAR);
}

static void inline set_bit(uint32_t *bit_map, int n)
{
	op_bits(bit_map, n, BITS_SET);
}

static int inline read_bit(uint32_t *bit_map, int n)
{
	return op_bits(bit_map, n, BITS_READ);
}

void init_bitmap(uint32_t bitmap[], int n);
int bitmap_find_free_base(uint32_t *map, int start,
		int value, int map_nr, int count);
#endif
