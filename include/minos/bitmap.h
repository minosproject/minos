#ifndef __LINUX_BITMAP_H
#define __LINUX_BITMAP_H

#include <minos/types.h>
#include <minos/bitops.h>
#include <minos/string.h>

/*
 * bitmaps provide bit arrays that consume one or more unsigned
 * longs.  The bitmap interface and available operations are listed
 * here, in bitmap.h
 *
 * Function implementations generic to all architectures are in
 * lib/bitmap.c.  Functions implementations that are architecture
 * specific are in various include/asm-<arch>/bitops.h headers
 * and other arch/<arch> specific files.
 *
 * See lib/bitmap.c for more details.
 */

/*
 * Also the following operations in asm/bitops.h apply to bitmaps.
 *
 * set_bit(bit, addr)			*addr |= bit
 * clear_bit(bit, addr)			*addr &= ~bit
 * change_bit(bit, addr)		*addr ^= bit
 * test_bit(bit, addr)			Is bit set in *addr?
 * test_and_set_bit(bit, addr)		Set bit and return old value
 * test_and_clear_bit(bit, addr)	Clear bit and return old value
 * test_and_change_bit(bit, addr)	Change bit and return old value
 * find_first_zero_bit(addr, nbits)	Position first zero bit in *addr
 * find_first_bit(addr, nbits)		Position first set bit in *addr
 * find_next_zero_bit(addr, nbits, bit)	Position next zero bit in *addr >= bit
 * find_next_bit(addr, nbits, bit)	Position next set bit in *addr >= bit
 */

/*
 * The DECLARE_BITMAP(name,bits) macro, in linux/types.h, can be used
 * to declare an array named 'name' of just enough unsigned longs to
 * contain all bit positions from 0 to 'bits' - 1.
 */

extern void bitmap_set(unsigned long *map, unsigned int start, int len);
extern void bitmap_clear(unsigned long *map, unsigned int start, int len);

extern unsigned long bitmap_find_next_zero_area_off(unsigned long *map,
						    unsigned long size,
						    unsigned long start,
						    unsigned int nr,
						    unsigned long align_mask,
						    unsigned long align_offset);

/**
 * bitmap_find_next_zero_area - find a contiguous aligned zero area
 * @map: The address to base the search on
 * @size: The bitmap size in bits
 * @start: The bitnumber to start searching at
 * @nr: The number of zeroed bits we're looking for
 * @align_mask: Alignment mask for zero area
 *
 * The @align_mask should be one less than a power of 2; the effect is that
 * the bit offset of all zero areas this function finds is multiples of that
 * power of 2. A @align_mask of 0 means no alignment is required.
 */
static inline unsigned long
bitmap_find_next_zero_area(unsigned long *map,
			   unsigned long size,
			   unsigned long start,
			   unsigned int nr,
			   unsigned long align_mask)
{
	return bitmap_find_next_zero_area_off(map, size, start, nr,
					      align_mask, 0);
}

unsigned long bitmap_find_next_zero_area_align(unsigned long *map,
					     unsigned long size,
					     unsigned long start,
					     unsigned int nr,
					     unsigned long align);

#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) & (BITS_PER_LONG - 1)))
#define BITMAP_LAST_WORD_MASK(nbits) (~0UL >> (-(nbits) & (BITS_PER_LONG - 1)))

#define small_const_nbits(nbits) \
	(__builtin_constant_p(nbits) && (nbits) <= BITS_PER_LONG)

static inline void bitmap_zero(unsigned long *dst, int nbits)
{
	if (small_const_nbits(nbits))
		*dst = 0UL;
	else {
		int len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
		memset((char *)dst, 0, len);
	}
}

static inline void bitmap_fill(unsigned long *dst, int nbits)
{
	size_t nlongs = BITS_TO_LONGS(nbits);
	if (!small_const_nbits(nbits)) {
		int len = (nlongs - 1) * sizeof(unsigned long);
		memset((char *)dst, 0xff,  len);
	}
	dst[nlongs - 1] = BITMAP_LAST_WORD_MASK(nbits);
}

#endif /* __LINUX_BITMAP_H */
