#include <asm/aarch64_common.h>
#include <asm/asm_marco.S>

	.global get_cpu_id

func get_cpu_id
	mrs 	x0, MPIDR_EL1
	ubfx    x0, x0, #MPIDR_EL1_AFF0_LSB, #MPIDR_EL1_AFF_WIDTH
	ret
endfunc get_cpu_id
