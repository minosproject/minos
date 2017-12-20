/*
 * Created by Le Min 2017/12/12
 */

#ifndef _MVISOR_CPU_H_
#define _MVISOR_CPU_H_

#include <core/types.h>

#define AARCH64_SPSR_EL3h 0b1101
#define AARCH64_SPSR_EL3t 0b1100
#define AARCH64_SPSR_EL2h 0b1001
#define AARCH64_SPSR_EL2t 0b1000
#define AARCH64_SPSR_EL1h 0b0101
#define AARCH64_SPSR_EL1t 0b0100
#define AARCH64_SPSR_EL0t 0b0000
#define AARCH64_SPSR_RW (1 << 4)
#define AARCH64_SPSR_F  (1 << 6)
#define AARCH64_SPSR_I  (1 << 7)
#define AARCH64_SPSR_A  (1 << 8)
#define AARCH64_SPSR_D  (1 << 9)
#define AARCH64_SPSR_IL (1 << 20)
#define AARCH64_SPSR_SS (1 << 21)
#define AARCH64_SPSR_V  (1 << 28)
#define AARCH64_SPSR_C  (1 << 29)
#define AARCH64_SPSR_Z  (1 << 30)
#define AARCH64_SPSR_N  (1 << 31)

int get_cpu_id(void);

static inline uint64_t
generate_vcpu_id(uint32_t pcpu_id, uint32_t vm_id, uint32_t vcpu_id)
{
	return ((vcpu_id) | (vm_id << 8) | (pcpu_id << 16));
}

#endif
