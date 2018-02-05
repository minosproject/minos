#include <mvisor/types.h>
#include <mvisor/errno.h>
#include <mvisor/bitmap.h>

static void inline get_bit_pos(int n, int *x, int *y)
{
	*x = n / (sizeof(uint32_t) * 8);
	*y = n % (sizeof(uint32_t) * 8);
}

int op_bits(uint32_t *bit_map, int n, bit_ops_t ops)
{
	int x, y, result = 0;

	if (!bit_map)
		return -EINVAL;

	get_bit_pos(n, &x, &y);
	switch (ops) {
		case BITS_CLEAR:
			bit_map[x] = bit_map[x] & (~(1 << y));
			break;
		case BITS_SET:
			bit_map[x] = bit_map[x] | (1 << y);
			break;
		case BITS_READ:
			result = (bit_map[x] & (1 << y)) >> y;
			break;
		default:
			result = -1;
			break;
	}

	return result;
}

void init_bitmap(uint32_t bitmap[], int n)
{
	int i;

	for (i = 0; i < n; i++) {
		clear_bit(bitmap, i);
	}
}

int bitmap_find_free_base(uint32_t *map, int start,
		int value, int map_nr, int count)
{
	int i = start;
	int again = 0;
	int sum = 0;

	while (1) {
		if (read_bit(map, i) == value) {
			sum++;
			if (sum == count)
				return (i + 1 - count);
		} else {
			sum = 0;
		}

		if (i == map_nr - 1) {
			again = 1;
			sum = 0;
			i = 0;
		}

		if (again) {
			if (i == start)
				break;
		}

		i++;
	}

	return -ENOSPC;
}
