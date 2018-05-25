#ifndef _MINOS_STDLIB_H_
#define _MINOS_STDLIB_H_

#include <minos/types.h>
#include <minos/math64.h>
#include <asm/div64.h>

#ifndef CONFIG_X86_64
#define mod_64(x, y) ((x) - (y) * div64_u64(x, y))
#else
#define mod_64(x, y) ((x) % (y))
#endif

uint64_t muldiv64(u64 a, u32 b, u32 c);

#endif
