#include <minos/types.h>
#include <minos/stdlib.h>
#include <minos/bitops.h>

#if 0
uint64_t div64_u64(uint64_t dividend, uint64_t divisor)
{
	uint32_t high = divisor >> 32;
	uint64_t quot;

	if (high == 0) {
		quot = div_u64(dividend, divisor);
	} else {
		int n = 1 + fls(high);
		quot = div_u64(dividend >> n, divisor >> n);

		if (quot != 0)
			quot--;
		if ((dividend - quot * divisor) >= divisor)
			quot++;
	}

	return quot;
}
#endif

/*
 * Compute with 96 bit intermediate result: (a*b)/c
 */
uint64_t muldiv64(uint64_t a, uint32_t b, uint32_t c)
{
	union {
		uint64_t ll;
		struct {
			uint32_t low, high;
		} l;
	} u, res;
	uint64_t rl, rh;

	u.ll = a;
	rl = (uint64_t)u.l.low * (uint64_t)b;
	rh = (uint64_t)u.l.high * (uint64_t)b;
	rh += (rl >> 32);
	res.l.high = div64_u64(rh, c);
	res.l.low = div64_u64(((mod_64(rh, c) << 32) + (rl & 0xffffffff)), c);
	return res.ll;
}
